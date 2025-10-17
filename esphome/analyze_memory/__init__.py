"""Memory usage analyzer for ESPHome compiled binaries."""

from collections import defaultdict
import json
import logging
from pathlib import Path
import re
import subprocess

from .const import DEMANGLED_PATTERNS, ESPHOME_COMPONENT_PATTERN, SYMBOL_PATTERNS

_LOGGER = logging.getLogger(__name__)


# Get the list of actual ESPHome components by scanning the components directory
def get_esphome_components():
    """Get set of actual ESPHome components from the components directory."""
    components = set()

    # Find the components directory relative to this file
    # Go up two levels from analyze_memory/__init__.py to esphome/
    current_dir = Path(__file__).parent.parent
    components_dir = current_dir / "components"

    if components_dir.exists() and components_dir.is_dir():
        for item in components_dir.iterdir():
            if (
                item.is_dir()
                and not item.name.startswith(".")
                and not item.name.startswith("__")
            ):
                components.add(item.name)

    return components


# Cache the component list
ESPHOME_COMPONENTS = get_esphome_components()


class MemorySection:
    """Represents a memory section with its symbols."""

    def __init__(self, name: str):
        self.name = name
        self.symbols: list[tuple[str, int, str]] = []  # (symbol_name, size, component)
        self.total_size = 0


class ComponentMemory:
    """Tracks memory usage for a component."""

    def __init__(self, name: str):
        self.name = name
        self.text_size = 0  # Code in flash
        self.rodata_size = 0  # Read-only data in flash
        self.data_size = 0  # Initialized data (flash + ram)
        self.bss_size = 0  # Uninitialized data (ram only)
        self.symbol_count = 0

    @property
    def flash_total(self) -> int:
        return self.text_size + self.rodata_size + self.data_size

    @property
    def ram_total(self) -> int:
        return self.data_size + self.bss_size


class MemoryAnalyzer:
    """Analyzes memory usage from ELF files."""

    def __init__(
        self,
        elf_path: str,
        objdump_path: str | None = None,
        readelf_path: str | None = None,
        external_components: set[str] | None = None,
    ):
        self.elf_path = Path(elf_path)
        if not self.elf_path.exists():
            raise FileNotFoundError(f"ELF file not found: {elf_path}")

        self.objdump_path = objdump_path or "objdump"
        self.readelf_path = readelf_path or "readelf"
        self.external_components = external_components or set()

        self.sections: dict[str, MemorySection] = {}
        self.components: dict[str, ComponentMemory] = defaultdict(
            lambda: ComponentMemory("")
        )
        self._demangle_cache: dict[str, str] = {}
        self._uncategorized_symbols: list[tuple[str, str, int]] = []
        self._esphome_core_symbols: list[
            tuple[str, str, int]
        ] = []  # Track core symbols
        self._component_symbols: dict[str, list[tuple[str, str, int]]] = defaultdict(
            list
        )  # Track symbols for all components

    def analyze(self) -> dict[str, ComponentMemory]:
        """Analyze the ELF file and return component memory usage."""
        self._parse_sections()
        self._parse_symbols()
        self._categorize_symbols()
        return dict(self.components)

    def _parse_sections(self) -> None:
        """Parse section headers from ELF file."""
        try:
            result = subprocess.run(
                [self.readelf_path, "-S", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            # Parse section headers
            for line in result.stdout.splitlines():
                # Look for section entries
                match = re.match(
                    r"\s*\[\s*\d+\]\s+([\.\w]+)\s+\w+\s+[\da-fA-F]+\s+[\da-fA-F]+\s+([\da-fA-F]+)",
                    line,
                )
                if match:
                    section_name = match.group(1)
                    size_hex = match.group(2)
                    size = int(size_hex, 16)

                    # Map various section names to standard categories
                    mapped_section = None
                    if ".text" in section_name or ".iram" in section_name:
                        mapped_section = ".text"
                    elif ".rodata" in section_name:
                        mapped_section = ".rodata"
                    elif ".data" in section_name and "bss" not in section_name:
                        mapped_section = ".data"
                    elif ".bss" in section_name:
                        mapped_section = ".bss"

                    if mapped_section:
                        if mapped_section not in self.sections:
                            self.sections[mapped_section] = MemorySection(
                                mapped_section
                            )
                        self.sections[mapped_section].total_size += size

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse sections: {e}")
            raise

    def _parse_symbols(self) -> None:
        """Parse symbols from ELF file."""
        # Section mapping - centralizes the logic
        SECTION_MAPPING = {
            ".text": [".text", ".iram"],
            ".rodata": [".rodata"],
            ".data": [".data", ".dram"],
            ".bss": [".bss"],
        }

        def map_section_name(raw_section: str) -> str | None:
            """Map raw section name to standard section."""
            for standard_section, patterns in SECTION_MAPPING.items():
                if any(pattern in raw_section for pattern in patterns):
                    return standard_section
            return None

        def parse_symbol_line(line: str) -> tuple[str, str, int, str] | None:
            """Parse a single symbol line from objdump output.

            Returns (section, name, size, address) or None if not a valid symbol.
            Format: address l/g w/d F/O section size name
            Example: 40084870 l     F .iram0.text    00000000 _xt_user_exc
            """
            parts = line.split()
            if len(parts) < 5:
                return None

            try:
                # Validate and extract address
                address = parts[0]
                int(address, 16)
            except ValueError:
                return None

            # Look for F (function) or O (object) flag
            if "F" not in parts and "O" not in parts:
                return None

            # Find section, size, and name
            for i, part in enumerate(parts):
                if part.startswith("."):
                    section = map_section_name(part)
                    if section and i + 1 < len(parts):
                        try:
                            size = int(parts[i + 1], 16)
                            if i + 2 < len(parts) and size > 0:
                                name = " ".join(parts[i + 2 :])
                                return (section, name, size, address)
                        except ValueError:
                            pass
                    break
            return None

        try:
            result = subprocess.run(
                [self.objdump_path, "-t", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            # Track seen addresses to avoid duplicates
            seen_addresses: set[str] = set()

            for line in result.stdout.splitlines():
                symbol_info = parse_symbol_line(line)
                if symbol_info:
                    section, name, size, address = symbol_info
                    # Skip duplicate symbols at the same address (e.g., C1/C2 constructors)
                    if address not in seen_addresses and section in self.sections:
                        self.sections[section].symbols.append((name, size, ""))
                        seen_addresses.add(address)

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse symbols: {e}")
            raise

    def _categorize_symbols(self) -> None:
        """Categorize symbols by component."""
        # First, collect all unique symbol names for batch demangling
        all_symbols = set()
        for section in self.sections.values():
            for symbol_name, _, _ in section.symbols:
                all_symbols.add(symbol_name)

        # Batch demangle all symbols at once
        self._batch_demangle_symbols(list(all_symbols))

        # Now categorize with cached demangled names
        for section_name, section in self.sections.items():
            for symbol_name, size, _ in section.symbols:
                component = self._identify_component(symbol_name)

                if component not in self.components:
                    self.components[component] = ComponentMemory(component)

                comp_mem = self.components[component]
                comp_mem.symbol_count += 1

                if section_name == ".text":
                    comp_mem.text_size += size
                elif section_name == ".rodata":
                    comp_mem.rodata_size += size
                elif section_name == ".data":
                    comp_mem.data_size += size
                elif section_name == ".bss":
                    comp_mem.bss_size += size

                # Track uncategorized symbols
                if component == "other" and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._uncategorized_symbols.append((symbol_name, demangled, size))

                # Track ESPHome core symbols for detailed analysis
                if component == "[esphome]core" and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._esphome_core_symbols.append((symbol_name, demangled, size))

                # Track all component symbols for detailed analysis
                if size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._component_symbols[component].append(
                        (symbol_name, demangled, size)
                    )

    def _identify_component(self, symbol_name: str) -> str:
        """Identify which component a symbol belongs to."""
        # Demangle C++ names if needed
        demangled = self._demangle_symbol(symbol_name)

        # Check for special component classes first (before namespace pattern)
        # This handles cases like esphome::ESPHomeOTAComponent which should map to ota
        if "esphome::" in demangled:
            # Check for special component classes that include component name in the class
            # For example: esphome::ESPHomeOTAComponent -> ota component
            for component_name in ESPHOME_COMPONENTS:
                # Check various naming patterns
                component_upper = component_name.upper()
                component_camel = component_name.replace("_", "").title()
                patterns = [
                    f"esphome::{component_upper}Component",  # e.g., esphome::OTAComponent
                    f"esphome::ESPHome{component_upper}Component",  # e.g., esphome::ESPHomeOTAComponent
                    f"esphome::{component_camel}Component",  # e.g., esphome::OtaComponent
                    f"esphome::ESPHome{component_camel}Component",  # e.g., esphome::ESPHomeOtaComponent
                ]

                if any(pattern in demangled for pattern in patterns):
                    return f"[esphome]{component_name}"

        # Check for ESPHome component namespaces
        match = ESPHOME_COMPONENT_PATTERN.search(demangled)
        if match:
            component_name = match.group(1)
            # Strip trailing underscore if present (e.g., switch_ -> switch)
            component_name = component_name.rstrip("_")

            # Check if this is an actual component in the components directory
            if component_name in ESPHOME_COMPONENTS:
                return f"[esphome]{component_name}"
            # Check if this is a known external component from the config
            if component_name in self.external_components:
                return f"[external]{component_name}"
            # Everything else in esphome:: namespace is core
            return "[esphome]core"

        # Check for esphome core namespace (no component namespace)
        if "esphome::" in demangled:
            # If no component match found, it's core
            return "[esphome]core"

        # Check against symbol patterns
        for component, patterns in SYMBOL_PATTERNS.items():
            if any(pattern in symbol_name for pattern in patterns):
                return component

        # Check against demangled patterns
        for component, patterns in DEMANGLED_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return component

        # Special cases that need more complex logic

        # Check if spi_flash vs spi_driver
        if "spi_" in symbol_name or "SPI" in symbol_name:
            if "spi_flash" in symbol_name:
                return "spi_flash"
            return "spi_driver"

        # libc special printf variants
        if symbol_name.startswith("_") and symbol_name[1:].replace("_r", "").replace(
            "v", ""
        ).replace("s", "") in ["printf", "fprintf", "sprintf", "scanf"]:
            return "libc"

        # Track uncategorized symbols for analysis
        return "other"

    def _batch_demangle_symbols(self, symbols: list[str]) -> None:
        """Batch demangle C++ symbol names for efficiency."""
        if not symbols:
            return

        # Try to find the appropriate c++filt for the platform
        cppfilt_cmd = "c++filt"

        # Check if we have a toolchain-specific c++filt
        if self.objdump_path and self.objdump_path != "objdump":
            # Replace objdump with c++filt in the path
            potential_cppfilt = self.objdump_path.replace("objdump", "c++filt")
            if Path(potential_cppfilt).exists():
                cppfilt_cmd = potential_cppfilt

        try:
            # Send all symbols to c++filt at once
            result = subprocess.run(
                [cppfilt_cmd],
                input="\n".join(symbols),
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0:
                demangled_lines = result.stdout.strip().split("\n")
                # Map original to demangled names
                for original, demangled in zip(symbols, demangled_lines):
                    self._demangle_cache[original] = demangled
            else:
                # If batch fails, cache originals
                for symbol in symbols:
                    self._demangle_cache[symbol] = symbol
        except Exception:
            # On error, cache originals
            for symbol in symbols:
                self._demangle_cache[symbol] = symbol

    def _demangle_symbol(self, symbol: str) -> str:
        """Get demangled C++ symbol name from cache."""
        return self._demangle_cache.get(symbol, symbol)

    def _categorize_esphome_core_symbol(self, demangled: str) -> str:
        """Categorize ESPHome core symbols into subcategories."""
        # Dictionary of patterns for core subcategories
        CORE_SUBCATEGORY_PATTERNS = {
            "Component Framework": ["Component"],
            "Application Core": ["Application"],
            "Scheduler": ["Scheduler"],
            "Logging": ["Logger", "log_"],
            "Preferences": ["preferences", "Preferences"],
            "Synchronization": ["Mutex", "Lock"],
            "Helpers": ["Helper"],
            "Network Utilities": ["network", "Network"],
            "Time Management": ["time", "Time"],
            "String Utilities": ["str_", "string"],
            "Parsing/Formatting": ["parse_", "format_"],
            "Optional Types": ["optional", "Optional"],
            "Callbacks": ["Callback", "callback"],
            "Color Utilities": ["Color"],
            "C++ Operators": ["operator"],
            "Global Variables": ["global_", "_GLOBAL"],
            "Setup/Loop": ["setup", "loop"],
            "System Control": ["reboot", "restart"],
            "GPIO Management": ["GPIO", "gpio"],
            "Interrupt Handling": ["ISR", "interrupt"],
            "Hooks": ["Hook", "hook"],
            "Entity Base Classes": ["Entity"],
            "Automation Framework": ["automation", "Automation"],
            "Automation Components": ["Condition", "Action", "Trigger"],
            "Lambda Support": ["lambda"],
        }

        # Special patterns that need to be checked separately
        if any(pattern in demangled for pattern in ["vtable", "typeinfo", "thunk"]):
            return "C++ Runtime (vtables/RTTI)"

        if demangled.startswith("std::"):
            return "C++ STL"

        # Check against patterns
        for category, patterns in CORE_SUBCATEGORY_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return category

        return "Other Core"

    def generate_report(self, detailed: bool = False) -> str:
        """Generate a formatted memory report."""
        components = sorted(
            self.components.items(), key=lambda x: x[1].flash_total, reverse=True
        )

        # Calculate totals
        total_flash = sum(c.flash_total for _, c in components)
        total_ram = sum(c.ram_total for _, c in components)

        # Build report
        lines = []

        # Column width constants
        COL_COMPONENT = 29
        COL_FLASH_TEXT = 14
        COL_FLASH_DATA = 14
        COL_RAM_DATA = 12
        COL_RAM_BSS = 12
        COL_TOTAL_FLASH = 15
        COL_TOTAL_RAM = 12
        COL_SEPARATOR = 3  # " | "

        # Core analysis column widths
        COL_CORE_SUBCATEGORY = 30
        COL_CORE_SIZE = 12
        COL_CORE_COUNT = 6
        COL_CORE_PERCENT = 10

        # Calculate the exact table width
        table_width = (
            COL_COMPONENT
            + COL_SEPARATOR
            + COL_FLASH_TEXT
            + COL_SEPARATOR
            + COL_FLASH_DATA
            + COL_SEPARATOR
            + COL_RAM_DATA
            + COL_SEPARATOR
            + COL_RAM_BSS
            + COL_SEPARATOR
            + COL_TOTAL_FLASH
            + COL_SEPARATOR
            + COL_TOTAL_RAM
        )

        lines.append("=" * table_width)
        lines.append("Component Memory Analysis".center(table_width))
        lines.append("=" * table_width)
        lines.append("")

        # Main table - fixed column widths
        lines.append(
            f"{'Component':<{COL_COMPONENT}} | {'Flash (text)':>{COL_FLASH_TEXT}} | {'Flash (data)':>{COL_FLASH_DATA}} | {'RAM (data)':>{COL_RAM_DATA}} | {'RAM (bss)':>{COL_RAM_BSS}} | {'Total Flash':>{COL_TOTAL_FLASH}} | {'Total RAM':>{COL_TOTAL_RAM}}"
        )
        lines.append(
            "-" * COL_COMPONENT
            + "-+-"
            + "-" * COL_FLASH_TEXT
            + "-+-"
            + "-" * COL_FLASH_DATA
            + "-+-"
            + "-" * COL_RAM_DATA
            + "-+-"
            + "-" * COL_RAM_BSS
            + "-+-"
            + "-" * COL_TOTAL_FLASH
            + "-+-"
            + "-" * COL_TOTAL_RAM
        )

        for name, mem in components:
            if mem.flash_total > 0 or mem.ram_total > 0:
                flash_rodata = mem.rodata_size + mem.data_size
                lines.append(
                    f"{name:<{COL_COMPONENT}} | {mem.text_size:>{COL_FLASH_TEXT - 2},} B | {flash_rodata:>{COL_FLASH_DATA - 2},} B | "
                    f"{mem.data_size:>{COL_RAM_DATA - 2},} B | {mem.bss_size:>{COL_RAM_BSS - 2},} B | "
                    f"{mem.flash_total:>{COL_TOTAL_FLASH - 2},} B | {mem.ram_total:>{COL_TOTAL_RAM - 2},} B"
                )

        lines.append(
            "-" * COL_COMPONENT
            + "-+-"
            + "-" * COL_FLASH_TEXT
            + "-+-"
            + "-" * COL_FLASH_DATA
            + "-+-"
            + "-" * COL_RAM_DATA
            + "-+-"
            + "-" * COL_RAM_BSS
            + "-+-"
            + "-" * COL_TOTAL_FLASH
            + "-+-"
            + "-" * COL_TOTAL_RAM
        )
        lines.append(
            f"{'TOTAL':<{COL_COMPONENT}} | {' ':>{COL_FLASH_TEXT}} | {' ':>{COL_FLASH_DATA}} | "
            f"{' ':>{COL_RAM_DATA}} | {' ':>{COL_RAM_BSS}} | "
            f"{total_flash:>{COL_TOTAL_FLASH - 2},} B | {total_ram:>{COL_TOTAL_RAM - 2},} B"
        )

        # Top consumers
        lines.append("")
        lines.append("Top Flash Consumers:")
        for i, (name, mem) in enumerate(components[:25]):
            if mem.flash_total > 0:
                percentage = (
                    (mem.flash_total / total_flash * 100) if total_flash > 0 else 0
                )
                lines.append(
                    f"{i + 1}. {name} ({mem.flash_total:,} B) - {percentage:.1f}% of analyzed flash"
                )

        lines.append("")
        lines.append("Top RAM Consumers:")
        ram_components = sorted(components, key=lambda x: x[1].ram_total, reverse=True)
        for i, (name, mem) in enumerate(ram_components[:25]):
            if mem.ram_total > 0:
                percentage = (mem.ram_total / total_ram * 100) if total_ram > 0 else 0
                lines.append(
                    f"{i + 1}. {name} ({mem.ram_total:,} B) - {percentage:.1f}% of analyzed RAM"
                )

        lines.append("")
        lines.append(
            "Note: This analysis covers symbols in the ELF file. Some runtime allocations may not be included."
        )
        lines.append("=" * table_width)

        # Add ESPHome core detailed analysis if there are core symbols
        if self._esphome_core_symbols:
            lines.append("")
            lines.append("=" * table_width)
            lines.append("[esphome]core Detailed Analysis".center(table_width))
            lines.append("=" * table_width)
            lines.append("")

            # Group core symbols by subcategory
            core_subcategories: dict[str, list[tuple[str, str, int]]] = defaultdict(
                list
            )

            for symbol, demangled, size in self._esphome_core_symbols:
                # Categorize based on demangled name patterns
                subcategory = self._categorize_esphome_core_symbol(demangled)
                core_subcategories[subcategory].append((symbol, demangled, size))

            # Sort subcategories by total size
            sorted_subcategories = sorted(
                [
                    (name, symbols, sum(s[2] for s in symbols))
                    for name, symbols in core_subcategories.items()
                ],
                key=lambda x: x[2],
                reverse=True,
            )

            lines.append(
                f"{'Subcategory':<{COL_CORE_SUBCATEGORY}} | {'Size':>{COL_CORE_SIZE}} | "
                f"{'Count':>{COL_CORE_COUNT}} | {'% of Core':>{COL_CORE_PERCENT}}"
            )
            lines.append(
                "-" * COL_CORE_SUBCATEGORY
                + "-+-"
                + "-" * COL_CORE_SIZE
                + "-+-"
                + "-" * COL_CORE_COUNT
                + "-+-"
                + "-" * COL_CORE_PERCENT
            )

            core_total = sum(size for _, _, size in self._esphome_core_symbols)

            for subcategory, symbols, total_size in sorted_subcategories:
                percentage = (total_size / core_total * 100) if core_total > 0 else 0
                lines.append(
                    f"{subcategory:<{COL_CORE_SUBCATEGORY}} | {total_size:>{COL_CORE_SIZE - 2},} B | "
                    f"{len(symbols):>{COL_CORE_COUNT}} | {percentage:>{COL_CORE_PERCENT - 1}.1f}%"
                )

            # Top 10 largest core symbols
            lines.append("")
            lines.append("Top 10 Largest [esphome]core Symbols:")
            sorted_core_symbols = sorted(
                self._esphome_core_symbols, key=lambda x: x[2], reverse=True
            )

            for i, (symbol, demangled, size) in enumerate(sorted_core_symbols[:15]):
                lines.append(f"{i + 1}. {demangled} ({size:,} B)")

            lines.append("=" * table_width)

        # Add detailed analysis for top ESPHome and external components
        esphome_components = [
            (name, mem)
            for name, mem in components
            if name.startswith("[esphome]") and name != "[esphome]core"
        ]
        external_components = [
            (name, mem) for name, mem in components if name.startswith("[external]")
        ]

        top_esphome_components = sorted(
            esphome_components, key=lambda x: x[1].flash_total, reverse=True
        )[:30]

        # Include all external components (they're usually important)
        top_external_components = sorted(
            external_components, key=lambda x: x[1].flash_total, reverse=True
        )

        # Check if API component exists and ensure it's included
        api_component = None
        for name, mem in components:
            if name == "[esphome]api":
                api_component = (name, mem)
                break

        # Combine all components to analyze: top ESPHome + all external + API if not already included
        components_to_analyze = list(top_esphome_components) + list(
            top_external_components
        )
        if api_component and api_component not in components_to_analyze:
            components_to_analyze.append(api_component)

        if components_to_analyze:
            for comp_name, comp_mem in components_to_analyze:
                comp_symbols = self._component_symbols.get(comp_name, [])
                if comp_symbols:
                    lines.append("")
                    lines.append("=" * table_width)
                    lines.append(f"{comp_name} Detailed Analysis".center(table_width))
                    lines.append("=" * table_width)
                    lines.append("")

                    # Sort symbols by size
                    sorted_symbols = sorted(
                        comp_symbols, key=lambda x: x[2], reverse=True
                    )

                    lines.append(f"Total symbols: {len(sorted_symbols)}")
                    lines.append(f"Total size: {comp_mem.flash_total:,} B")
                    lines.append("")

                    # Show all symbols > 100 bytes for better visibility
                    large_symbols = [
                        (sym, dem, size)
                        for sym, dem, size in sorted_symbols
                        if size > 100
                    ]

                    lines.append(
                        f"{comp_name} Symbols > 100 B ({len(large_symbols)} symbols):"
                    )
                    for i, (symbol, demangled, size) in enumerate(large_symbols):
                        lines.append(f"{i + 1}. {demangled} ({size:,} B)")

                    lines.append("=" * table_width)

        return "\n".join(lines)

    def to_json(self) -> str:
        """Export analysis results as JSON."""
        data = {
            "components": {
                name: {
                    "text": mem.text_size,
                    "rodata": mem.rodata_size,
                    "data": mem.data_size,
                    "bss": mem.bss_size,
                    "flash_total": mem.flash_total,
                    "ram_total": mem.ram_total,
                    "symbol_count": mem.symbol_count,
                }
                for name, mem in self.components.items()
            },
            "totals": {
                "flash": sum(c.flash_total for c in self.components.values()),
                "ram": sum(c.ram_total for c in self.components.values()),
            },
        }
        return json.dumps(data, indent=2)

    def dump_uncategorized_symbols(self, output_file: str | None = None) -> None:
        """Dump uncategorized symbols for analysis."""
        # Sort by size descending
        sorted_symbols = sorted(
            self._uncategorized_symbols, key=lambda x: x[2], reverse=True
        )

        lines = ["Uncategorized Symbols Analysis", "=" * 80]
        lines.append(f"Total uncategorized symbols: {len(sorted_symbols)}")
        lines.append(
            f"Total uncategorized size: {sum(s[2] for s in sorted_symbols):,} bytes"
        )
        lines.append("")
        lines.append(f"{'Size':>10} | {'Symbol':<60} | Demangled")
        lines.append("-" * 10 + "-+-" + "-" * 60 + "-+-" + "-" * 40)

        for symbol, demangled, size in sorted_symbols[:100]:  # Top 100
            if symbol != demangled:
                lines.append(f"{size:>10,} | {symbol[:60]:<60} | {demangled[:100]}")
            else:
                lines.append(f"{size:>10,} | {symbol[:60]:<60} | [not demangled]")

        if len(sorted_symbols) > 100:
            lines.append(f"\n... and {len(sorted_symbols) - 100} more symbols")

        content = "\n".join(lines)

        if output_file:
            with open(output_file, "w") as f:
                f.write(content)
        else:
            print(content)


def analyze_elf(
    elf_path: str,
    objdump_path: str | None = None,
    readelf_path: str | None = None,
    detailed: bool = False,
    external_components: set[str] | None = None,
) -> str:
    """Analyze an ELF file and return a memory report."""
    analyzer = MemoryAnalyzer(elf_path, objdump_path, readelf_path, external_components)
    analyzer.analyze()
    return analyzer.generate_report(detailed)


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: analyze_memory.py <elf_file>")
        sys.exit(1)

    try:
        report = analyze_elf(sys.argv[1])
        print(report)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
