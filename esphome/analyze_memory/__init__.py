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
    SYMBOL_PATTERNS,
)
from .demangle import batch_demangle
from .helpers import (
    get_component_class_patterns,
    get_esphome_components,
    map_section_name,
    parse_symbol_line,
)
from .toolchain import find_tool, resolve_tool_path, run_tool

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
_COMPONENT_PREFIX_LIB = "[lib]"
_COMPONENT_CORE = f"{_COMPONENT_PREFIX_ESPHOME}core"
_COMPONENT_API = f"{_COMPONENT_PREFIX_ESPHOME}api"

# C++ namespace prefixes
_NAMESPACE_ESPHOME = "esphome::"
_NAMESPACE_STD = "std::"

# Type alias for symbol information: (symbol_name, size, component)
SymbolInfoType = tuple[str, int, str]

# RAM sections - symbols in these sections consume RAM
RAM_SECTIONS = frozenset([".data", ".bss"])

# nm symbol types for global/weak defined symbols (used for library symbol mapping)
# Only global (uppercase) and weak symbols are safe to use - local symbols (lowercase)
# can have name collisions across compilation units
_NM_DEFINED_GLOBAL_TYPES = frozenset({"T", "D", "B", "R", "W", "V"})

# Pattern matching compiler-generated local names that can collide across compilation
# units (e.g., packet$19, buf$20, flag$5261). These are unsafe for name-based lookup.
# Does NOT match mangled C++ names with optimization suffixes (e.g., func$isra$0).
_COMPILER_LOCAL_PATTERN = re.compile(r"^[a-zA-Z_]\w*\$\d+$")


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

    def add_section_size(self, section_name: str, size: int) -> None:
        """Add size to the appropriate attribute for a section."""
        if section_name == ".text":
            self.text_size += size
        elif section_name == ".rodata":
            self.rodata_size += size
        elif section_name == ".data":
            self.data_size += size
        elif section_name == ".bss":
            self.bss_size += size

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

        # Validate paths exist, fall back to find_tool if they don't
        # This handles cases like Zephyr where cc_path doesn't include full path
        # and the toolchain prefix may differ (e.g., arm-zephyr-eabi- vs arm-none-eabi-)
        objdump_path = resolve_tool_path("objdump", objdump_path, objdump_path)
        readelf_path = resolve_tool_path("readelf", readelf_path, objdump_path)

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
        # CSWTCH symbols: list of (name, size, source_file, component)
        self._cswtch_symbols: list[tuple[str, int, str, str]] = []
        # Library symbol mapping: symbol_name -> library_name
        self._lib_symbol_map: dict[str, str] = {}
        # Library dir to name mapping: "lib641" -> "espsoftwareserial",
        # "espressif__mdns" -> "mdns"
        self._lib_hash_to_name: dict[str, str] = {}
        # Heuristic category to library redirect: "mdns_lib" -> "[lib]mdns"
        self._heuristic_to_lib: dict[str, str] = {}

    def analyze(self) -> dict[str, ComponentMemory]:
        """Analyze the ELF file and return component memory usage."""
        self._parse_sections()
        self._parse_symbols()
        self._scan_libraries()
        self._categorize_symbols()
        self._analyze_cswtch_symbols()
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
                comp_mem.add_section_size(section_name, size)

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

        # Check library symbol map (more accurate than heuristic patterns)
        if lib_name := self._lib_symbol_map.get(symbol_name):
            return f"{_COMPONENT_PREFIX_LIB}{lib_name}"

        # Check against symbol patterns
        for component, patterns in SYMBOL_PATTERNS.items():
            if any(pattern in symbol_name for pattern in patterns):
                return self._heuristic_to_lib.get(component, component)

        # Check against demangled patterns
        for component, patterns in DEMANGLED_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return self._heuristic_to_lib.get(component, component)

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

    def _discover_pio_libraries(
        self,
        libraries: dict[str, list[Path]],
        hash_to_name: dict[str, str],
    ) -> None:
        """Discover PlatformIO third-party libraries from the build directory.

        Scans ``lib<hex>/`` directories under ``.pioenvs/<env>/`` to find
        library names and their ``.a`` archive or ``.o`` file paths.

        Args:
            libraries: Dict to populate with library name -> file path list mappings.
                Prefers ``.a`` archives when available, falls back to ``.o`` files
                (e.g., pioarduino ESP32 Arduino builds only produce ``.o`` files).
            hash_to_name: Dict to populate with dir name -> library name mappings
                for CSWTCH attribution (e.g., ``lib641`` -> ``espsoftwareserial``).
        """
        build_dir = self.elf_path.parent

        for entry in build_dir.iterdir():
            if not entry.is_dir() or not entry.name.startswith("lib"):
                continue
            # Validate that the suffix after "lib" is a hex hash
            hex_part = entry.name[3:]
            if not hex_part:
                continue
            try:
                int(hex_part, 16)
            except ValueError:
                continue

            # Each lib<hex>/ directory contains a subdirectory named after the library
            for lib_subdir in entry.iterdir():
                if not lib_subdir.is_dir():
                    continue
                lib_name = lib_subdir.name.lower()

                # Prefer .a archive (lib<LibraryName>.a), fall back to .o files
                # e.g., lib72a/ESPAsyncTCP/... has lib72a/libESPAsyncTCP.a
                archive = entry / f"lib{lib_subdir.name}.a"
                if archive.exists():
                    file_paths = [archive]
                elif archives := list(entry.glob("*.a")):
                    # Case-insensitive fallback
                    file_paths = [archives[0]]
                else:
                    # No .a archive (e.g., pioarduino CMake builds) - use .o files
                    file_paths = sorted(lib_subdir.rglob("*.o"))

                if file_paths:
                    libraries[lib_name] = file_paths
                    hash_to_name[entry.name] = lib_name
                    _LOGGER.debug(
                        "Discovered PlatformIO library: %s -> %s",
                        lib_subdir.name,
                        file_paths[0],
                    )

    def _discover_idf_managed_components(
        self,
        libraries: dict[str, list[Path]],
        hash_to_name: dict[str, str],
    ) -> None:
        """Discover ESP-IDF managed component libraries from the build directory.

        ESP-IDF managed components (from the IDF component registry) use a
        ``<vendor>__<name>`` naming convention. Source files live under
        ``managed_components/<vendor>__<name>/`` and the compiled archives are at
        ``esp-idf/<vendor>__<name>/lib<vendor>__<name>.a``.

        Args:
            libraries: Dict to populate with library name -> file path list mappings.
            hash_to_name: Dict to populate with dir name -> library name mappings
                for CSWTCH attribution (e.g., ``espressif__mdns`` -> ``mdns``).
        """
        build_dir = self.elf_path.parent

        managed_dir = build_dir / "managed_components"
        if not managed_dir.is_dir():
            return

        espidf_dir = build_dir / "esp-idf"

        for entry in managed_dir.iterdir():
            if not entry.is_dir() or "__" not in entry.name:
                continue

            # Extract the short name: espressif__mdns -> mdns
            full_name = entry.name  # e.g., espressif__mdns
            short_name = full_name.split("__", 1)[1].lower()

            # Find the .a archive under esp-idf/<vendor>__<name>/
            archive = espidf_dir / full_name / f"lib{full_name}.a"
            if archive.exists():
                libraries[short_name] = [archive]
                hash_to_name[full_name] = short_name
                _LOGGER.debug(
                    "Discovered IDF managed component: %s -> %s",
                    short_name,
                    archive,
                )

    def _build_library_symbol_map(
        self, libraries: dict[str, list[Path]]
    ) -> dict[str, str]:
        """Build a symbol-to-library mapping from library archives or object files.

        Runs ``nm --defined-only`` on each ``.a`` or ``.o`` file to collect
        global and weak defined symbols.

        Args:
            libraries: Dictionary mapping library name to list of file paths
                (``.a`` archives or ``.o`` object files).

        Returns:
            Dictionary mapping symbol name to library name.
        """
        symbol_map: dict[str, str] = {}

        if not self.nm_path:
            return symbol_map

        for lib_name, file_paths in libraries.items():
            result = run_tool(
                [self.nm_path, "--defined-only", *(str(p) for p in file_paths)],
                timeout=10,
            )
            if result is None or result.returncode != 0:
                continue

            for line in result.stdout.splitlines():
                parts = line.split()
                if len(parts) < 3:
                    continue

                sym_type = parts[-2]
                sym_name = parts[-1]

                # Include global defined symbols (uppercase) and weak symbols (W/V)
                if sym_type in _NM_DEFINED_GLOBAL_TYPES:
                    symbol_map[sym_name] = lib_name

        return symbol_map

    @staticmethod
    def _build_heuristic_to_lib_mapping(
        library_names: set[str],
    ) -> dict[str, str]:
        """Build mapping from heuristic pattern categories to discovered libraries.

        Heuristic categories like ``mdns_lib``, ``web_server_lib``, ``async_tcp``
        exist as approximations for library attribution.  When we discover the
        actual library, symbols matching those heuristics should be redirected
        to the ``[lib]`` category instead.

        The mapping is built by checking if the normalized category name
        (stripped of ``_lib`` suffix and underscores) appears as a substring
        of any discovered library name.

        Examples::

            mdns_lib -> mdns -> in "mdns" or "esp8266mdns" -> [lib]mdns
            web_server_lib -> webserver -> in "espasyncwebserver" -> [lib]espasyncwebserver
            async_tcp -> asynctcp -> in "espasynctcp" -> [lib]espasynctcp

        Args:
            library_names: Set of discovered library names (lowercase).

        Returns:
            Dictionary mapping heuristic category to ``[lib]<name>`` string.
        """
        mapping: dict[str, str] = {}
        all_categories = set(SYMBOL_PATTERNS) | set(DEMANGLED_PATTERNS)

        for category in all_categories:
            base = category.removesuffix("_lib").replace("_", "")
            # Collect all libraries whose name contains the base string
            candidates = [lib_name for lib_name in library_names if base in lib_name]
            if not candidates:
                continue

            # Choose a deterministic "best" match:
            #   1. Prefer exact name matches over substring matches.
            #   2. Among non-exact matches, prefer the shortest library name.
            #   3. Break remaining ties lexicographically.
            best_lib = min(
                candidates,
                key=lambda lib_name, _base=base: (
                    lib_name != _base,
                    len(lib_name),
                    lib_name,
                ),
            )
            mapping[category] = f"{_COMPONENT_PREFIX_LIB}{best_lib}"

        if mapping:
            _LOGGER.debug(
                "Heuristic-to-library redirects: %s",
                ", ".join(f"{k} -> {v}" for k, v in sorted(mapping.items())),
            )

        return mapping

    def _parse_map_file(self) -> dict[str, str] | None:
        """Parse linker map file to build authoritative symbol-to-library mapping.

        The linker map file contains the definitive source attribution for every
        symbol, including local/static ones that ``nm`` cannot safely export.

        Map file format (GNU ld)::

            .text._mdns_service_task
                        0x400e9fdc      0x65c  .pioenvs/env/esp-idf/espressif__mdns/libespressif__mdns.a(mdns.c.o)

        Each section entry has a ``.section.symbol_name`` line followed by an
        indented line with address, size, and source path.

        Returns:
            Symbol-to-library dict, or ``None`` if no usable map file exists.
        """
        map_path = self.elf_path.with_suffix(".map")
        if not map_path.exists() or map_path.stat().st_size < 10000:
            return None

        _LOGGER.info("Parsing linker map file: %s", map_path.name)

        try:
            map_text = map_path.read_text(encoding="utf-8", errors="replace")
        except OSError as err:
            _LOGGER.warning("Failed to read map file: %s", err)
            return None

        symbol_map: dict[str, str] = {}
        current_symbol: str | None = None
        section_prefixes = (".text.", ".rodata.", ".data.", ".bss.", ".literal.")

        for line in map_text.splitlines():
            # Match section.symbol line: " .text.symbol_name"
            # Single space indent, starts with dot
            if len(line) > 2 and line[0] == " " and line[1] == ".":
                stripped = line.strip()
                for prefix in section_prefixes:
                    if stripped.startswith(prefix):
                        current_symbol = stripped[len(prefix) :]
                        break
                else:
                    current_symbol = None
                continue

            # Match source attribution line: "        0xADDR  0xSIZE  source_path"
            if current_symbol is None:
                continue

            fields = line.split()
            # Skip compiler-generated local names (e.g., packet$19, buf$20)
            # that can collide across compilation units
            if (
                len(fields) >= 3
                and fields[0].startswith("0x")
                and fields[1].startswith("0x")
                and not _COMPILER_LOCAL_PATTERN.match(current_symbol)
            ):
                source_path = fields[2]
                # Check if source path contains a known library directory
                for dir_key, lib_name in self._lib_hash_to_name.items():
                    if dir_key in source_path:
                        symbol_map[current_symbol] = lib_name
                        break

            current_symbol = None

        return symbol_map or None

    def _scan_libraries(self) -> None:
        """Discover third-party libraries and build symbol mapping.

        Scans both PlatformIO ``lib<hex>/`` directories (Arduino builds) and
        ESP-IDF ``managed_components/`` (IDF builds) to find library archives.

        Uses the linker map file for authoritative symbol attribution when
        available, falling back to ``nm`` scanning with heuristic redirects.
        """
        libraries: dict[str, list[Path]] = {}
        self._discover_pio_libraries(libraries, self._lib_hash_to_name)
        self._discover_idf_managed_components(libraries, self._lib_hash_to_name)

        if not libraries:
            _LOGGER.debug("No third-party libraries found")
            return

        _LOGGER.info(
            "Scanning %d libraries: %s",
            len(libraries),
            ", ".join(sorted(libraries)),
        )

        # Heuristic redirect catches local symbols (e.g., mdns_task_buffer$14)
        # that can't be safely added to the symbol map due to name collisions
        self._heuristic_to_lib = self._build_heuristic_to_lib_mapping(
            set(libraries.keys())
        )

        # Try linker map file first (authoritative, includes local symbols)
        map_symbols = self._parse_map_file()
        if map_symbols is not None:
            self._lib_symbol_map = map_symbols
            _LOGGER.info(
                "Built library symbol map from linker map: %d symbols",
                len(self._lib_symbol_map),
            )
            return

        # Fall back to nm scanning (global symbols only)
        self._lib_symbol_map = self._build_library_symbol_map(libraries)

        _LOGGER.info(
            "Built library symbol map from nm: %d symbols from %d libraries",
            len(self._lib_symbol_map),
            len(libraries),
        )

    def _find_object_files_dir(self) -> Path | None:
        """Find the directory containing object files for this build.

        Returns:
            Path to the directory containing .o files, or None if not found.
        """
        # The ELF is typically at .pioenvs/<env>/firmware.elf
        # Object files are in .pioenvs/<env>/src/ and .pioenvs/<env>/lib*/
        pioenvs_dir = self.elf_path.parent
        if pioenvs_dir.exists() and any(pioenvs_dir.glob("src/*.o")):
            return pioenvs_dir
        return None

    @staticmethod
    def _parse_nm_cswtch_output(
        output: str,
        base_dir: Path | None,
        cswtch_map: dict[str, list[tuple[str, int]]],
    ) -> None:
        """Parse nm output for CSWTCH symbols and add to cswtch_map.

        Handles both ``.o`` files and ``.a`` archives.

        nm output formats::

            .o files:  /path/file.o:hex_addr hex_size type name
            .a files:  /path/lib.a:member.o:hex_addr hex_size type name

        For ``.o`` files, paths are made relative to *base_dir* when possible.
        For ``.a`` archives (detected by ``:`` in the file portion), paths are
        formatted as ``archive_stem/member.o`` (e.g. ``liblwip2-536-feat/lwip-esp.o``).

        Args:
            output: Raw stdout from ``nm --print-file-name -S``.
            base_dir: Base directory for computing relative paths of ``.o`` files.
                      Pass ``None`` when scanning archives outside the build tree.
            cswtch_map: Dict to populate, mapping ``"CSWTCH$N:size"`` to source list.
        """
        for line in output.splitlines():
            if "CSWTCH$" not in line:
                continue

            # Split on last ":" that precedes a hex address.
            # For .o:  "filepath.o" : "hex_addr hex_size type name"
            # For .a:  "filepath.a:member.o" : "hex_addr hex_size type name"
            parts_after_colon = line.rsplit(":", 1)
            if len(parts_after_colon) != 2:
                continue

            file_path = parts_after_colon[0]
            fields = parts_after_colon[1].split()
            # fields: [address, size, type, name]
            if len(fields) < 4:
                continue

            sym_name = fields[3]
            if not sym_name.startswith("CSWTCH$"):
                continue

            try:
                size = int(fields[1], 16)
            except ValueError:
                continue

            # Determine readable source path
            # Use ".a:" to detect archive format (not bare ":" which matches
            # Windows drive letters like "C:\...\file.o").
            if ".a:" in file_path:
                # Archive format: "archive.a:member.o" → "archive_stem/member.o"
                archive_part, member = file_path.rsplit(":", 1)
                archive_name = Path(archive_part).stem
                rel_path = f"{archive_name}/{member}"
            elif base_dir is not None:
                try:
                    rel_path = str(Path(file_path).relative_to(base_dir))
                except ValueError:
                    rel_path = file_path
            else:
                rel_path = file_path

            key = f"{sym_name}:{size}"
            cswtch_map[key].append((rel_path, size))

    def _run_nm_cswtch_scan(
        self,
        files: list[Path],
        base_dir: Path | None,
        cswtch_map: dict[str, list[tuple[str, int]]],
    ) -> None:
        """Run nm on *files* and add any CSWTCH symbols to *cswtch_map*.

        Args:
            files: Object (``.o``) or archive (``.a``) files to scan.
            base_dir: Base directory for relative path computation (see
                      :meth:`_parse_nm_cswtch_output`).
            cswtch_map: Dict to populate with results.
        """
        if not self.nm_path or not files:
            return

        _LOGGER.debug("Scanning %d files for CSWTCH symbols", len(files))

        result = run_tool(
            [self.nm_path, "--print-file-name", "-S"] + [str(f) for f in files],
            timeout=30,
        )
        if result is None or result.returncode != 0:
            _LOGGER.debug(
                "nm failed or timed out scanning %d files for CSWTCH symbols",
                len(files),
            )
            return

        self._parse_nm_cswtch_output(result.stdout, base_dir, cswtch_map)

    def _scan_cswtch_in_sdk_archives(
        self, cswtch_map: dict[str, list[tuple[str, int]]]
    ) -> None:
        """Scan SDK library archives (.a) for CSWTCH symbols.

        Prebuilt SDK libraries (e.g. lwip, bearssl) are not compiled from source,
        so their CSWTCH symbols only exist inside ``.a`` archives.  Results are
        merged into *cswtch_map* for keys not already found in ``.o`` files.

        The same source file (e.g. ``lwip-esp.o``) often appears in multiple
        library variants (``liblwip2-536.a``, ``liblwip2-1460-feat.a``, etc.),
        so results are deduplicated by member name.
        """
        sdk_dirs = self._find_sdk_library_dirs()
        if not sdk_dirs:
            return

        sdk_archives = sorted(a for sdk_dir in sdk_dirs for a in sdk_dir.glob("*.a"))

        sdk_map: dict[str, list[tuple[str, int]]] = defaultdict(list)
        self._run_nm_cswtch_scan(sdk_archives, None, sdk_map)

        # Merge SDK results, deduplicating by member name.
        for key, sources in sdk_map.items():
            if key in cswtch_map:
                continue
            seen: dict[str, tuple[str, int]] = {}
            for path, sz in sources:
                member = Path(path).name
                if member not in seen:
                    seen[member] = (path, sz)
            cswtch_map[key] = list(seen.values())

    def _source_file_to_component(self, source_file: str) -> str:
        """Map a source object file path to its component name.

        Args:
            source_file: Relative path like 'src/esphome/components/wifi/wifi_component.cpp.o'

        Returns:
            Component name like '[esphome]wifi' or the source file if unknown.
        """
        parts = Path(source_file).parts

        # ESPHome component: src/esphome/components/<name>/...
        if "components" in parts:
            idx = parts.index("components")
            if idx + 1 < len(parts):
                component_name = parts[idx + 1]
                if component_name in get_esphome_components():
                    return f"{_COMPONENT_PREFIX_ESPHOME}{component_name}"
                if component_name in self.external_components:
                    return f"{_COMPONENT_PREFIX_EXTERNAL}{component_name}"

        # ESPHome core: src/esphome/core/... or src/esphome/...
        if "core" in parts and "esphome" in parts:
            return _COMPONENT_CORE
        if "esphome" in parts and "components" not in parts:
            return _COMPONENT_CORE

        # Framework/library files - check for PlatformIO library hash dirs
        # e.g., lib65b/ESPAsyncTCP/... -> [lib]espasynctcp
        if parts and parts[0] in self._lib_hash_to_name:
            return f"{_COMPONENT_PREFIX_LIB}{self._lib_hash_to_name[parts[0]]}"

        # ESP-IDF managed components: managed_components/espressif__mdns/... -> [lib]mdns
        if (
            len(parts) >= 2
            and parts[0] == "managed_components"
            and parts[1] in self._lib_hash_to_name
        ):
            return f"{_COMPONENT_PREFIX_LIB}{self._lib_hash_to_name[parts[1]]}"

        # Other framework/library files - return the first path component
        # e.g., FrameworkArduino/... -> FrameworkArduino
        return parts[0] if parts else source_file

    def _analyze_cswtch_symbols(self) -> None:
        """Analyze CSWTCH (GCC switch table) symbols by tracing to source objects.

        CSWTCH symbols are compiler-generated lookup tables for switch statements.
        They are local symbols, so the same name can appear in different object files.
        This method scans .o files and SDK archives to attribute them to their
        source components.
        """
        obj_dir = self._find_object_files_dir()
        if obj_dir is None:
            _LOGGER.debug("No object files directory found, skipping CSWTCH analysis")
            return

        # Scan build-dir object files for CSWTCH symbols
        cswtch_map: dict[str, list[tuple[str, int]]] = defaultdict(list)
        self._run_nm_cswtch_scan(sorted(obj_dir.rglob("*.o")), obj_dir, cswtch_map)

        # Also scan SDK library archives (.a) for CSWTCH symbols.
        # Prebuilt SDK libraries (e.g. lwip, bearssl) are not compiled from source
        # so their symbols only exist inside .a archives, not as loose .o files.
        self._scan_cswtch_in_sdk_archives(cswtch_map)

        if not cswtch_map:
            _LOGGER.debug("No CSWTCH symbols found in object files or SDK archives")
            return

        # Collect CSWTCH symbols from the ELF (already parsed in sections)
        # Include section_name for re-attribution of component totals
        elf_cswtch = [
            (symbol_name, size, section_name)
            for section_name, section in self.sections.items()
            for symbol_name, size, _ in section.symbols
            if symbol_name.startswith("CSWTCH$")
        ]

        _LOGGER.debug(
            "Found %d CSWTCH symbols in ELF, %d unique in object files",
            len(elf_cswtch),
            len(cswtch_map),
        )

        # Match ELF CSWTCH symbols to source files and re-attribute component totals.
        # _categorize_symbols() already ran and put these into "other" since CSWTCH$
        # names don't match any component pattern. We move the bytes to the correct
        # component based on the object file mapping.
        other_mem = self.components.get("other")

        for sym_name, size, section_name in elf_cswtch:
            key = f"{sym_name}:{size}"
            sources = cswtch_map.get(key, [])

            if len(sources) == 1:
                source_file = sources[0][0]
                component = self._source_file_to_component(source_file)
            elif len(sources) > 1:
                # Ambiguous - multiple object files have same CSWTCH name+size
                source_file = "ambiguous"
                component = "ambiguous"
                _LOGGER.debug(
                    "Ambiguous CSWTCH %s (%d B) found in %d files: %s",
                    sym_name,
                    size,
                    len(sources),
                    ", ".join(src for src, _ in sources),
                )
            else:
                source_file = "unknown"
                component = "unknown"

            self._cswtch_symbols.append((sym_name, size, source_file, component))

            # Re-attribute from "other" to the correct component
            if (
                component not in ("other", "unknown", "ambiguous")
                and other_mem is not None
            ):
                other_mem.add_section_size(section_name, -size)
                if component not in self.components:
                    self.components[component] = ComponentMemory(component)
                self.components[component].add_section_size(section_name, size)

        # Sort by size descending
        self._cswtch_symbols.sort(key=lambda x: x[1], reverse=True)

        total_size = sum(size for _, size, _, _ in self._cswtch_symbols)
        _LOGGER.debug(
            "CSWTCH analysis: %d symbols, %d bytes total",
            len(self._cswtch_symbols),
            total_size,
        )

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
