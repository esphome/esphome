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


@dataclass
class MemorySection:
    """Represents a memory section with its symbols."""

    name: str
    symbols: list[SymbolInfoType] = field(default_factory=list)
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
                        (symbol_name, demangled, size)
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


if __name__ == "__main__":
    from .cli import main

    main()
