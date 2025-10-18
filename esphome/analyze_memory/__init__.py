"""Memory usage analyzer for ESPHome compiled binaries."""

from collections import defaultdict
from dataclasses import dataclass, field
from functools import cache
import json
import logging
from pathlib import Path
import re
import subprocess

from .const import (
    CORE_SUBCATEGORY_PATTERNS,
    DEMANGLED_PATTERNS,
    ESPHOME_COMPONENT_PATTERN,
    SYMBOL_PATTERNS,
)
from .helpers import map_section_name, parse_symbol_line

_LOGGER = logging.getLogger(__name__)


# Get the list of actual ESPHome components by scanning the components directory
@cache
def get_esphome_components():
    """Get set of actual ESPHome components from the components directory."""
    # Find the components directory relative to this file
    # Go up two levels from analyze_memory/__init__.py to esphome/
    current_dir = Path(__file__).parent.parent
    components_dir = current_dir / "components"

    if not components_dir.exists() or not components_dir.is_dir():
        return frozenset()

    return frozenset(
        item.name
        for item in components_dir.iterdir()
        if item.is_dir()
        and not item.name.startswith(".")
        and not item.name.startswith("__")
    )


@cache
def get_component_class_patterns(component_name: str) -> list[str]:
    """Generate component class name patterns for symbol matching.

    Args:
        component_name: The component name (e.g., "ota", "wifi", "api")

    Returns:
        List of pattern strings to match against demangled symbols
    """
    component_upper = component_name.upper()
    component_camel = component_name.replace("_", "").title()
    return [
        f"esphome::{component_upper}Component",  # e.g., esphome::OTAComponent
        f"esphome::ESPHome{component_upper}Component",  # e.g., esphome::ESPHomeOTAComponent
        f"esphome::{component_camel}Component",  # e.g., esphome::OtaComponent
        f"esphome::ESPHome{component_camel}Component",  # e.g., esphome::ESPHomeOtaComponent
    ]


@dataclass
class MemorySection:
    """Represents a memory section with its symbols."""

    name: str
    symbols: list[tuple[str, int, str]] = field(
        default_factory=list
    )  # (symbol_name, size, component)
    total_size: int = 0


@dataclass
class ComponentMemory:
    """Tracks memory usage for a component."""

    name: str
    text_size: int = 0  # Code in flash
    rodata_size: int = 0  # Read-only data in flash
    data_size: int = 0  # Initialized data (flash + ram)
    bss_size: int = 0  # Uninitialized data (ram only)
    symbol_count: int = 0

    @property
    def flash_total(self) -> int:
        """Total flash usage (text + rodata + data)."""
        return self.text_size + self.rodata_size + self.data_size

    @property
    def ram_total(self) -> int:
        """Total RAM usage (data + bss)."""
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

                    # Map to standard section name
                    mapped_section = map_section_name(section_name)
                    if mapped_section:
                        if mapped_section not in self.sections:
                            self.sections[mapped_section] = MemorySection(
                                mapped_section
                            )
                        self.sections[mapped_section].total_size += size

        except subprocess.CalledProcessError as e:
            _LOGGER.error("Failed to parse sections: %s", e)
            raise

    def _parse_symbols(self) -> None:
        """Parse symbols from ELF file."""
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
                if not (symbol_info := parse_symbol_line(line)):
                    continue

                section, name, size, address = symbol_info

                # Skip duplicate symbols at the same address (e.g., C1/C2 constructors)
                if address in seen_addresses or section not in self.sections:
                    continue

                self.sections[section].symbols.append((name, size, ""))
                seen_addresses.add(address)

        except subprocess.CalledProcessError as e:
            _LOGGER.error("Failed to parse symbols: %s", e)
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
            for component_name in get_esphome_components():
                patterns = get_component_class_patterns(component_name)
                if any(pattern in demangled for pattern in patterns):
                    return f"[esphome]{component_name}"

        # Check for ESPHome component namespaces
        match = ESPHOME_COMPONENT_PATTERN.search(demangled)
        if match:
            component_name = match.group(1)
            # Strip trailing underscore if present (e.g., switch_ -> switch)
            component_name = component_name.rstrip("_")

            # Check if this is an actual component in the components directory
            if component_name in get_esphome_components():
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
        except (subprocess.SubprocessError, OSError, UnicodeDecodeError) as e:
            # On error, cache originals
            _LOGGER.debug("Failed to batch demangle symbols: %s", e)
            for symbol in symbols:
                self._demangle_cache[symbol] = symbol

    def _demangle_symbol(self, symbol: str) -> str:
        """Get demangled C++ symbol name from cache."""
        return self._demangle_cache.get(symbol, symbol)

    def _categorize_esphome_core_symbol(self, demangled: str) -> str:
        """Categorize ESPHome core symbols into subcategories."""
        # Special patterns that need to be checked separately
        if any(pattern in demangled for pattern in ["vtable", "typeinfo", "thunk"]):
            return "C++ Runtime (vtables/RTTI)"

        if demangled.startswith("std::"):
            return "C++ STL"

        # Check against patterns from const.py
        for category, patterns in CORE_SUBCATEGORY_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return category

        return "Other Core"

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


if __name__ == "__main__":
    from .cli import main

    main()
