"""Memory usage analyzer for ESPHome compiled binaries."""

from collections import defaultdict
from dataclasses import dataclass, field
import logging
from pathlib import Path
import re
import subprocess
from typing import TYPE_CHECKING

from .const import (
    CORE_SUBCATEGORY_PATTERNS,
    DEMANGLED_PATTERNS,
    ESPHOME_COMPONENT_PATTERN,
    SECTION_TO_ATTR,
    SYMBOL_PATTERNS,
)
from .demangle import batch_demangle
from .helpers import (
    get_component_class_patterns,
    get_esphome_components,
    map_section_name,
    parse_symbol_line,
)
from .toolchain import find_tool, run_tool

if TYPE_CHECKING:
    from esphome.platformio_api import IDEData

_LOGGER = logging.getLogger(__name__)

# C++ runtime patterns for categorization
_CPP_RUNTIME_PATTERNS = frozenset(["vtable", "typeinfo", "thunk"])

# libc printf/scanf family base names (used to detect variants like _printf_r, vfprintf, etc.)
_LIBC_PRINTF_SCANF_FAMILY = frozenset(["printf", "fprintf", "sprintf", "scanf"])

# Regex pattern for parsing readelf section headers
# Format: [ #] name type addr off size
_READELF_SECTION_PATTERN = re.compile(
    r"\s*\[\s*\d+\]\s+([\.\w]+)\s+\w+\s+[\da-fA-F]+\s+[\da-fA-F]+\s+([\da-fA-F]+)"
)

# Component category prefixes
_COMPONENT_PREFIX_ESPHOME = "[esphome]"
_COMPONENT_PREFIX_EXTERNAL = "[external]"
_COMPONENT_CORE = f"{_COMPONENT_PREFIX_ESPHOME}core"
_COMPONENT_API = f"{_COMPONENT_PREFIX_ESPHOME}api"

# C++ namespace prefixes
_NAMESPACE_ESPHOME = "esphome::"
_NAMESPACE_STD = "std::"

# Type alias for symbol information: (symbol_name, size, component)
SymbolInfoType = tuple[str, int, str]

# RAM sections - symbols in these sections consume RAM
RAM_SECTIONS = frozenset([".data", ".bss"])


@dataclass
class MemorySection:
    """Represents a memory section with its symbols."""

    name: str
    symbols: list[SymbolInfoType] = field(default_factory=list)
    total_size: int = 0  # Actual section size from ELF headers
    symbol_size: int = 0  # Sum of symbol sizes (may be less than total_size)


@dataclass
class SDKSymbol:
    """Represents a symbol from an SDK library that's not in the ELF symbol table."""

    name: str
    size: int
    library: str  # Name of the .a file (e.g., "libpp.a")
    section: str  # ".bss" or ".data"
    is_local: bool  # True if static/local symbol (lowercase in nm output)
    demangled: str = ""  # Demangled name (populated after analysis)


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
        idedata: "IDEData | None" = None,
    ) -> None:
        """Initialize memory analyzer.

        Args:
            elf_path: Path to ELF file to analyze
            objdump_path: Path to objdump binary (auto-detected from idedata if not provided)
            readelf_path: Path to readelf binary (auto-detected from idedata if not provided)
            external_components: Set of external component names
            idedata: Optional PlatformIO IDEData object to auto-detect toolchain paths
        """
        self.elf_path = Path(elf_path)
        if not self.elf_path.exists():
            raise FileNotFoundError(f"ELF file not found: {elf_path}")

        # Auto-detect toolchain paths from idedata if not provided
        if idedata is not None and (objdump_path is None or readelf_path is None):
            objdump_path = objdump_path or idedata.objdump_path
            readelf_path = readelf_path or idedata.readelf_path
            _LOGGER.debug("Using toolchain paths from PlatformIO idedata")

        self.objdump_path = objdump_path or "objdump"
        self.readelf_path = readelf_path or "readelf"
        self.external_components = external_components or set()
        self._idedata = idedata

        # Derive nm path from objdump path using shared toolchain utility
        self.nm_path = find_tool("nm", self.objdump_path)

        self.sections: dict[str, MemorySection] = {}
        self.components: dict[str, ComponentMemory] = defaultdict(
            lambda: ComponentMemory("")
        )
        self._demangle_cache: dict[str, str] = {}
        self._uncategorized_symbols: list[tuple[str, str, int]] = []
        self._esphome_core_symbols: list[
            tuple[str, str, int]
        ] = []  # Track core symbols
        # Track symbols for all components: (symbol_name, demangled, size, section)
        self._component_symbols: dict[str, list[tuple[str, str, int, str]]] = (
            defaultdict(list)
        )
        # Track RAM symbols separately for detailed analysis: (symbol_name, demangled, size, section)
        self._ram_symbols: dict[str, list[tuple[str, str, int, str]]] = defaultdict(
            list
        )
        # Track ELF symbol names for SDK cross-reference
        self._elf_symbol_names: set[str] = set()
        # SDK symbols not in ELF (static/local symbols from closed-source libs)
        self._sdk_symbols: list[SDKSymbol] = []

    def analyze(self) -> dict[str, ComponentMemory]:
        """Analyze the ELF file and return component memory usage."""
        self._parse_sections()
        self._parse_symbols()
        self._categorize_symbols()
        self._analyze_sdk_libraries()
        return dict(self.components)

    def _parse_sections(self) -> None:
        """Parse section headers from ELF file."""
        result = subprocess.run(
            [self.readelf_path, "-S", str(self.elf_path)],
            capture_output=True,
            text=True,
            check=True,
        )

        # Parse section headers
        for line in result.stdout.splitlines():
            # Look for section entries
            if not (match := _READELF_SECTION_PATTERN.match(line)):
                continue

            section_name = match.group(1)
            size_hex = match.group(2)
            size = int(size_hex, 16)

            # Map to standard section name
            mapped_section = map_section_name(section_name)
            if not mapped_section:
                continue

            if mapped_section not in self.sections:
                self.sections[mapped_section] = MemorySection(mapped_section)
            self.sections[mapped_section].total_size += size

    def _parse_symbols(self) -> None:
        """Parse symbols from ELF file."""
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
            self.sections[section].symbol_size += size
            self._elf_symbol_names.add(name)
            seen_addresses.add(address)

    def _categorize_symbols(self) -> None:
        """Categorize symbols by component."""
        # First, collect all unique symbol names for batch demangling
        all_symbols = {
            symbol_name
            for section in self.sections.values()
            for symbol_name, _, _ in section.symbols
        }

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

                # Update the appropriate size attribute based on section
                if attr_name := SECTION_TO_ATTR.get(section_name):
                    setattr(comp_mem, attr_name, getattr(comp_mem, attr_name) + size)

                # Track uncategorized symbols
                if component == "other" and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._uncategorized_symbols.append((symbol_name, demangled, size))

                # Track ESPHome core symbols for detailed analysis
                if component == _COMPONENT_CORE and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._esphome_core_symbols.append((symbol_name, demangled, size))

                # Track all component symbols for detailed analysis
                if size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._component_symbols[component].append(
                        (symbol_name, demangled, size, section_name)
                    )
                    # Track RAM symbols separately for detailed RAM analysis
                    if section_name in RAM_SECTIONS:
                        self._ram_symbols[component].append(
                            (symbol_name, demangled, size, section_name)
                        )

    def _identify_component(self, symbol_name: str) -> str:
        """Identify which component a symbol belongs to."""
        # Demangle C++ names if needed
        demangled = self._demangle_symbol(symbol_name)

        # Check for special component classes first (before namespace pattern)
        # This handles cases like esphome::ESPHomeOTAComponent which should map to ota
        if _NAMESPACE_ESPHOME in demangled:
            # Check for special component classes that include component name in the class
            # For example: esphome::ESPHomeOTAComponent -> ota component
            for component_name in get_esphome_components():
                patterns = get_component_class_patterns(component_name)
                if any(pattern in demangled for pattern in patterns):
                    return f"{_COMPONENT_PREFIX_ESPHOME}{component_name}"

        # Check for ESPHome component namespaces
        match = ESPHOME_COMPONENT_PATTERN.search(demangled)
        if match:
            component_name = match.group(1)
            # Strip trailing underscore if present (e.g., switch_ -> switch)
            component_name = component_name.rstrip("_")

            # Check if this is an actual component in the components directory
            if component_name in get_esphome_components():
                return f"{_COMPONENT_PREFIX_ESPHOME}{component_name}"
            # Check if this is a known external component from the config
            if component_name in self.external_components:
                return f"{_COMPONENT_PREFIX_EXTERNAL}{component_name}"
            # Everything else in esphome:: namespace is core
            return _COMPONENT_CORE

        # Check for esphome core namespace (no component namespace)
        if _NAMESPACE_ESPHOME in demangled:
            # If no component match found, it's core
            return _COMPONENT_CORE

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
            return "spi_flash" if "spi_flash" in symbol_name else "spi_driver"

        # libc special printf variants
        if (
            symbol_name.startswith("_")
            and symbol_name[1:].replace("_r", "").replace("v", "").replace("s", "")
            in _LIBC_PRINTF_SCANF_FAMILY
        ):
            return "libc"

        # Track uncategorized symbols for analysis
        return "other"

    def _batch_demangle_symbols(self, symbols: list[str]) -> None:
        """Batch demangle C++ symbol names for efficiency."""
        if not symbols:
            return

        _LOGGER.info("Demangling %d symbols", len(symbols))
        self._demangle_cache = batch_demangle(symbols, objdump_path=self.objdump_path)
        _LOGGER.info("Successfully demangled %d symbols", len(self._demangle_cache))

    def _demangle_symbol(self, symbol: str) -> str:
        """Get demangled C++ symbol name from cache."""
        return self._demangle_cache.get(symbol, symbol)

    def _categorize_esphome_core_symbol(self, demangled: str) -> str:
        """Categorize ESPHome core symbols into subcategories."""
        # Special patterns that need to be checked separately
        if any(pattern in demangled for pattern in _CPP_RUNTIME_PATTERNS):
            return "C++ Runtime (vtables/RTTI)"

        if demangled.startswith(_NAMESPACE_STD):
            return "C++ STL"

        # Check against patterns from const.py
        for category, patterns in CORE_SUBCATEGORY_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return category

        return "Other Core"

    def get_unattributed_ram(self) -> tuple[int, int, int]:
        """Get unattributed RAM sizes (SDK/framework overhead).

        Returns:
            Tuple of (unattributed_bss, unattributed_data, total_unattributed)
            These are bytes in RAM sections that have no corresponding symbols.
        """
        bss_section = self.sections.get(".bss")
        data_section = self.sections.get(".data")

        unattributed_bss = 0
        unattributed_data = 0

        if bss_section:
            unattributed_bss = max(0, bss_section.total_size - bss_section.symbol_size)
        if data_section:
            unattributed_data = max(
                0, data_section.total_size - data_section.symbol_size
            )

        return unattributed_bss, unattributed_data, unattributed_bss + unattributed_data

    def _find_sdk_library_dirs(self) -> list[Path]:
        """Find SDK library directories based on platform.

        Returns:
            List of paths to SDK library directories containing .a files.
        """
        sdk_dirs: list[Path] = []

        if self._idedata is None:
            return sdk_dirs

        # Get the CC path to determine the framework location
        cc_path = getattr(self._idedata, "cc_path", None)
        if not cc_path:
            return sdk_dirs

        cc_path = Path(cc_path)

        # For ESP8266 Arduino framework
        # CC is like: ~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-gcc
        # SDK libs are in: ~/.platformio/packages/framework-arduinoespressif8266/tools/sdk/lib/
        if "xtensa-lx106" in str(cc_path):
            platformio_dir = cc_path.parent.parent.parent
            esp8266_sdk = (
                platformio_dir
                / "framework-arduinoespressif8266"
                / "tools"
                / "sdk"
                / "lib"
            )
            if esp8266_sdk.exists():
                sdk_dirs.append(esp8266_sdk)
                # Also check for NONOSDK subdirectories (closed-source libs)
                sdk_dirs.extend(
                    subdir
                    for subdir in esp8266_sdk.iterdir()
                    if subdir.is_dir() and subdir.name.startswith("NONOSDK")
                )

        # For ESP32 IDF framework
        # CC is like: ~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-gcc
        # or: ~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-gcc
        elif "xtensa-esp" in str(cc_path) or "riscv32-esp" in str(cc_path):
            # Detect ESP32 variant from CC path or defines
            variant = self._detect_esp32_variant()
            if variant:
                platformio_dir = cc_path.parent.parent.parent
                espidf_dir = platformio_dir / "framework-espidf" / "components"
                if espidf_dir.exists():
                    # Find all directories named after the variant that contain .a files
                    # This handles various ESP-IDF library layouts:
                    # - components/*/lib/<variant>/
                    # - components/*/<variant>/
                    # - components/*/lib/lib/<variant>/
                    # - components/*/*/lib_*/<variant>/
                    sdk_dirs.extend(
                        variant_dir
                        for variant_dir in espidf_dir.rglob(variant)
                        if variant_dir.is_dir() and any(variant_dir.glob("*.a"))
                    )

        return sdk_dirs

    def _detect_esp32_variant(self) -> str | None:
        """Detect ESP32 variant from idedata defines.

        Returns:
            Variant string like 'esp32', 'esp32s2', 'esp32c3', etc. or None.
        """
        if self._idedata is None:
            return None

        defines = getattr(self._idedata, "defines", [])
        if not defines:
            return None

        # ESPHome always adds USE_ESP32_VARIANT_xxx defines
        variant_prefix = "USE_ESP32_VARIANT_"
        for define in defines:
            if define.startswith(variant_prefix):
                # Extract variant name and convert to lowercase
                # USE_ESP32_VARIANT_ESP32 -> esp32
                # USE_ESP32_VARIANT_ESP32S3 -> esp32s3
                return define[len(variant_prefix) :].lower()

        return None

    def _parse_sdk_library(
        self, lib_path: Path
    ) -> tuple[list[tuple[str, int, str, bool]], set[str]]:
        """Parse a single SDK library for symbols.

        Args:
            lib_path: Path to the .a library file

        Returns:
            Tuple of:
            - List of BSS/DATA symbols: (symbol_name, size, section, is_local)
            - Set of global BSS/DATA symbol names (for checking if RAM is linked)
        """
        ram_symbols: list[tuple[str, int, str, bool]] = []
        global_ram_symbols: set[str] = set()

        result = run_tool([self.nm_path, "--size-sort", str(lib_path)], timeout=10)
        if result is None:
            return ram_symbols, global_ram_symbols

        for line in result.stdout.splitlines():
            parts = line.split()
            if len(parts) < 3:
                continue

            try:
                size = int(parts[0], 16)
                sym_type = parts[1]
                name = parts[2]

                # Only collect BSS (b/B) and DATA (d/D) for RAM analysis
                if sym_type in ("b", "B"):
                    section = ".bss"
                    is_local = sym_type == "b"
                    ram_symbols.append((name, size, section, is_local))
                    # Track global RAM symbols (B/D) for linking check
                    if sym_type == "B":
                        global_ram_symbols.add(name)
                elif sym_type in ("d", "D"):
                    section = ".data"
                    is_local = sym_type == "d"
                    ram_symbols.append((name, size, section, is_local))
                    if sym_type == "D":
                        global_ram_symbols.add(name)
            except (ValueError, IndexError):
                continue

        return ram_symbols, global_ram_symbols

    def _analyze_sdk_libraries(self) -> None:
        """Analyze SDK libraries to find symbols not in the ELF.

        This finds static/local symbols from closed-source SDK libraries
        that consume RAM but don't appear in the final ELF symbol table.
        Only includes symbols from libraries that have RAM actually linked
        (at least one global BSS/DATA symbol in the ELF).
        """
        sdk_dirs = self._find_sdk_library_dirs()
        if not sdk_dirs:
            _LOGGER.debug("No SDK library directories found")
            return

        _LOGGER.debug("Analyzing SDK libraries in %d directories", len(sdk_dirs))

        # Track seen symbols to avoid duplicates from multiple SDK versions
        seen_symbols: set[str] = set()

        for sdk_dir in sdk_dirs:
            for lib_path in sorted(sdk_dir.glob("*.a")):
                lib_name = lib_path.name
                ram_symbols, global_ram_symbols = self._parse_sdk_library(lib_path)

                # Check if this library's RAM is actually linked by seeing if any
                # of its global BSS/DATA symbols appear in the ELF
                if not global_ram_symbols & self._elf_symbol_names:
                    # No RAM from this library is in the ELF - skip it
                    continue

                for name, size, section, is_local in ram_symbols:
                    # Skip if already in ELF or already seen from another lib
                    if name in self._elf_symbol_names or name in seen_symbols:
                        continue

                    # Only track symbols with non-zero size
                    if size > 0:
                        self._sdk_symbols.append(
                            SDKSymbol(
                                name=name,
                                size=size,
                                library=lib_name,
                                section=section,
                                is_local=is_local,
                            )
                        )
                        seen_symbols.add(name)

        # Demangle SDK symbols for better readability
        if self._sdk_symbols:
            sdk_names = [sym.name for sym in self._sdk_symbols]
            demangled_map = batch_demangle(sdk_names, objdump_path=self.objdump_path)
            for sym in self._sdk_symbols:
                sym.demangled = demangled_map.get(sym.name, sym.name)

        # Sort by size descending for reporting
        self._sdk_symbols.sort(key=lambda s: s.size, reverse=True)

        total_sdk_ram = sum(s.size for s in self._sdk_symbols)
        _LOGGER.debug(
            "Found %d SDK symbols not in ELF, totaling %d bytes",
            len(self._sdk_symbols),
            total_sdk_ram,
        )

    def get_sdk_ram_symbols(self) -> list[SDKSymbol]:
        """Get SDK symbols that consume RAM but aren't in the ELF symbol table.

        Returns:
            List of SDKSymbol objects sorted by size descending.
        """
        return self._sdk_symbols

    def get_sdk_ram_by_library(self) -> dict[str, list[SDKSymbol]]:
        """Get SDK RAM symbols grouped by library.

        Returns:
            Dictionary mapping library name to list of symbols.
        """
        by_lib: dict[str, list[SDKSymbol]] = defaultdict(list)
        for sym in self._sdk_symbols:
            by_lib[sym.library].append(sym)
        return dict(by_lib)


if __name__ == "__main__":
    from .cli import main

    main()
