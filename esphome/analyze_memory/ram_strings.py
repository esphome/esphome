"""Analyzer for RAM-stored strings in ESP8266/ESP32 firmware ELF files.

This module identifies strings that are stored in RAM sections (.data, .bss, .rodata)
rather than in flash sections (.irom0.text, .irom.text), which is important for
memory-constrained platforms like ESP8266.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
import logging
from pathlib import Path
import re
import subprocess

from .demangle import batch_demangle
from .toolchain import find_tool

_LOGGER = logging.getLogger(__name__)

# ESP8266: .rodata is in RAM (DRAM), not flash
# ESP32: .rodata is in flash, mapped to data bus
ESP8266_RAM_SECTIONS = frozenset([".data", ".rodata", ".bss"])
ESP8266_FLASH_SECTIONS = frozenset([".irom0.text", ".irom.text", ".text"])

# ESP32: .rodata is memory-mapped from flash
ESP32_RAM_SECTIONS = frozenset([".data", ".bss", ".dram0.data", ".dram0.bss"])
ESP32_FLASH_SECTIONS = frozenset([".text", ".rodata", ".flash.text", ".flash.rodata"])

# nm symbol types for data symbols (D=global data, d=local data, R=rodata, B=bss)
DATA_SYMBOL_TYPES = frozenset(["D", "d", "R", "r", "B", "b"])


@dataclass
class SectionInfo:
    """Information about an ELF section."""

    name: str
    address: int
    size: int


@dataclass
class RamString:
    """A string found in RAM."""

    section: str
    address: int
    content: str

    @property
    def size(self) -> int:
        """Size in bytes including null terminator."""
        return len(self.content) + 1


@dataclass
class RamSymbol:
    """A symbol found in RAM."""

    name: str
    sym_type: str
    address: int
    size: int
    section: str
    demangled: str = ""  # Demangled name, set after batch demangling


class RamStringsAnalyzer:
    """Analyzes ELF files to find strings stored in RAM."""

    def __init__(
        self,
        elf_path: str,
        objdump_path: str | None = None,
        min_length: int = 8,
        platform: str = "esp32",
    ) -> None:
        """Initialize the RAM strings analyzer.

        Args:
            elf_path: Path to the ELF file to analyze
            objdump_path: Path to objdump binary (used to find other tools)
            min_length: Minimum string length to report (default: 8)
            platform: Platform name ("esp8266", "esp32", etc.) for section mapping
        """
        self.elf_path = Path(elf_path)
        if not self.elf_path.exists():
            raise FileNotFoundError(f"ELF file not found: {elf_path}")

        self.objdump_path = objdump_path
        self.min_length = min_length
        self.platform = platform

        # Set RAM/flash sections based on platform
        if self.platform == "esp8266":
            self.ram_sections = ESP8266_RAM_SECTIONS
            self.flash_sections = ESP8266_FLASH_SECTIONS
        else:
            # ESP32 and other platforms
            self.ram_sections = ESP32_RAM_SECTIONS
            self.flash_sections = ESP32_FLASH_SECTIONS

        self.sections: dict[str, SectionInfo] = {}
        self.ram_strings: list[RamString] = []
        self.ram_symbols: list[RamSymbol] = []

    def _run_command(self, cmd: list[str]) -> str:
        """Run a command and return its output."""
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            return result.stdout
        except subprocess.CalledProcessError as e:
            _LOGGER.debug("Command failed: %s - %s", " ".join(cmd), e.stderr)
            raise
        except FileNotFoundError:
            _LOGGER.warning("Command not found: %s", cmd[0])
            raise

    def analyze(self) -> None:
        """Perform the full RAM analysis."""
        self._parse_sections()
        self._extract_strings()
        self._analyze_symbols()
        self._demangle_symbols()

    def _parse_sections(self) -> None:
        """Parse section headers from ELF file."""
        objdump = find_tool("objdump", self.objdump_path)
        if not objdump:
            _LOGGER.error("Could not find objdump command")
            return

        try:
            output = self._run_command([objdump, "-h", str(self.elf_path)])
        except (subprocess.CalledProcessError, FileNotFoundError):
            return

        # Parse section headers
        # Format: Idx Name          Size      VMA       LMA       File off  Algn
        section_pattern = re.compile(
            r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)"
        )

        for line in output.split("\n"):
            if match := section_pattern.match(line):
                name = match.group(1)
                size = int(match.group(2), 16)
                vma = int(match.group(3), 16)
                self.sections[name] = SectionInfo(name, vma, size)

    def _extract_strings(self) -> None:
        """Extract strings from RAM sections."""
        objdump = find_tool("objdump", self.objdump_path)
        if not objdump:
            return

        for section_name in self.ram_sections:
            if section_name not in self.sections:
                continue

            try:
                output = self._run_command(
                    [objdump, "-s", "-j", section_name, str(self.elf_path)]
                )
            except subprocess.CalledProcessError:
                # Section may exist but have no content (e.g., .bss)
                continue
            except FileNotFoundError:
                continue

            strings = self._parse_hex_dump(output, section_name)
            self.ram_strings.extend(strings)

    def _parse_hex_dump(self, output: str, section_name: str) -> list[RamString]:
        """Parse hex dump output to extract strings.

        Args:
            output: Output from objdump -s
            section_name: Name of the section being parsed

        Returns:
            List of RamString objects
        """
        strings: list[RamString] = []
        current_string = bytearray()
        string_start_addr = 0

        for line in output.split("\n"):
            # Lines look like: " 3ffef8a0 00000000 00000000 00000000 00000000  ................"
            match = re.match(r"^\s+([0-9a-fA-F]+)\s+((?:[0-9a-fA-F]{2,8}\s*)+)", line)
            if not match:
                continue

            addr = int(match.group(1), 16)
            hex_data = match.group(2).strip()

            # Convert hex to bytes
            hex_bytes = hex_data.split()
            byte_offset = 0
            for hex_chunk in hex_bytes:
                # Handle both byte-by-byte and word formats
                for i in range(0, len(hex_chunk), 2):
                    byte_val = int(hex_chunk[i : i + 2], 16)
                    if 0x20 <= byte_val <= 0x7E:  # Printable ASCII
                        if not current_string:
                            string_start_addr = addr + byte_offset
                        current_string.append(byte_val)
                    else:
                        if byte_val == 0 and len(current_string) >= self.min_length:
                            # Found null terminator
                            strings.append(
                                RamString(
                                    section=section_name,
                                    address=string_start_addr,
                                    content=current_string.decode(
                                        "ascii", errors="ignore"
                                    ),
                                )
                            )
                        current_string = bytearray()
                    byte_offset += 1

        return strings

    def _analyze_symbols(self) -> None:
        """Analyze symbols in RAM sections."""
        nm = find_tool("nm", self.objdump_path)
        if not nm:
            return

        try:
            output = self._run_command([nm, "-S", "--size-sort", str(self.elf_path)])
        except (subprocess.CalledProcessError, FileNotFoundError):
            return

        for line in output.split("\n"):
            parts = line.split()
            if len(parts) < 4:
                continue

            try:
                addr = int(parts[0], 16)
                size = int(parts[1], 16) if parts[1] != "?" else 0
            except ValueError:
                continue

            sym_type = parts[2]
            name = " ".join(parts[3:])

            # Filter for data symbols
            if sym_type not in DATA_SYMBOL_TYPES:
                continue

            # Check if symbol is in a RAM section
            for section_name in self.ram_sections:
                if section_name not in self.sections:
                    continue

                section = self.sections[section_name]
                if section.address <= addr < section.address + section.size:
                    self.ram_symbols.append(
                        RamSymbol(
                            name=name,
                            sym_type=sym_type,
                            address=addr,
                            size=size,
                            section=section_name,
                        )
                    )
                    break

    def _demangle_symbols(self) -> None:
        """Batch demangle all RAM symbol names."""
        if not self.ram_symbols:
            return

        # Collect all symbol names and demangle them
        symbol_names = [s.name for s in self.ram_symbols]
        demangle_cache = batch_demangle(symbol_names, objdump_path=self.objdump_path)

        # Assign demangled names to symbols
        for symbol in self.ram_symbols:
            symbol.demangled = demangle_cache.get(symbol.name, symbol.name)

    def _get_sections_size(self, section_names: frozenset[str]) -> int:
        """Get total size of specified sections."""
        return sum(
            section.size
            for name, section in self.sections.items()
            if name in section_names
        )

    def get_total_ram_usage(self) -> int:
        """Get total RAM usage from RAM sections."""
        return self._get_sections_size(self.ram_sections)

    def get_total_flash_usage(self) -> int:
        """Get total flash usage from flash sections."""
        return self._get_sections_size(self.flash_sections)

    def get_total_string_bytes(self) -> int:
        """Get total bytes used by strings in RAM."""
        return sum(s.size for s in self.ram_strings)

    def get_repeated_strings(self) -> list[tuple[str, int]]:
        """Find strings that appear multiple times.

        Returns:
            List of (string, count) tuples sorted by potential savings
        """
        string_counts: dict[str, int] = defaultdict(int)
        for ram_string in self.ram_strings:
            string_counts[ram_string.content] += 1

        return sorted(
            [(s, c) for s, c in string_counts.items() if c > 1],
            key=lambda x: x[1] * (len(x[0]) + 1),
            reverse=True,
        )

    def get_long_strings(self, min_len: int = 20) -> list[RamString]:
        """Get strings longer than the specified length.

        Args:
            min_len: Minimum string length

        Returns:
            List of RamString objects sorted by length
        """
        return sorted(
            [s for s in self.ram_strings if len(s.content) >= min_len],
            key=lambda x: len(x.content),
            reverse=True,
        )

    def get_largest_symbols(self, min_size: int = 100) -> list[RamSymbol]:
        """Get RAM symbols larger than the specified size.

        Args:
            min_size: Minimum symbol size in bytes

        Returns:
            List of RamSymbol objects sorted by size
        """
        return sorted(
            [s for s in self.ram_symbols if s.size >= min_size],
            key=lambda x: x.size,
            reverse=True,
        )

    def generate_report(self, show_all_sections: bool = False) -> str:
        """Generate a formatted RAM strings analysis report.

        Args:
            show_all_sections: If True, show all sections, not just RAM

        Returns:
            Formatted report string
        """
        lines: list[str] = []
        table_width = 80

        lines.append("=" * table_width)
        lines.append(
            f"RAM Strings Analysis ({self.platform.upper()})".center(table_width)
        )
        lines.append("=" * table_width)
        lines.append("")

        # Section Analysis
        lines.append("SECTION ANALYSIS")
        lines.append("-" * table_width)
        lines.append(f"{'Section':<20} {'Address':<12} {'Size':<12} {'Location'}")
        lines.append("-" * table_width)

        total_ram_usage = 0
        total_flash_usage = 0

        for name, section in sorted(self.sections.items(), key=lambda x: x[1].address):
            if name in self.ram_sections:
                location = "RAM"
                total_ram_usage += section.size
            elif name in self.flash_sections:
                location = "FLASH"
                total_flash_usage += section.size
            else:
                location = "OTHER"

            if show_all_sections or name in self.ram_sections:
                lines.append(
                    f"{name:<20} 0x{section.address:08x}   {section.size:>8} B   {location}"
                )

        lines.append("-" * table_width)
        lines.append(f"Total RAM sections size: {total_ram_usage:,} bytes")
        lines.append(f"Total Flash sections size: {total_flash_usage:,} bytes")

        # Strings in RAM
        lines.append("")
        lines.append("=" * table_width)
        lines.append("STRINGS IN RAM SECTIONS")
        lines.append("=" * table_width)
        lines.append(
            "Note: .bss sections contain uninitialized data (no strings to extract)"
        )

        # Group strings by section
        strings_by_section: dict[str, list[RamString]] = defaultdict(list)
        for ram_string in self.ram_strings:
            strings_by_section[ram_string.section].append(ram_string)

        for section_name in sorted(strings_by_section.keys()):
            section_strings = strings_by_section[section_name]
            lines.append(f"\nSection: {section_name}")
            lines.append("-" * 40)
            for ram_string in sorted(section_strings, key=lambda x: x.address):
                clean_string = ram_string.content[:100] + (
                    "..." if len(ram_string.content) > 100 else ""
                )
                lines.append(
                    f'  0x{ram_string.address:08x}: "{clean_string}" (len={len(ram_string.content)})'
                )

        # Large RAM symbols
        lines.append("")
        lines.append("=" * table_width)
        lines.append("LARGE DATA SYMBOLS IN RAM (>= 50 bytes)")
        lines.append("=" * table_width)

        largest_symbols = self.get_largest_symbols(50)
        lines.append(f"\n{'Symbol':<50} {'Type':<6} {'Size':<10} {'Section'}")
        lines.append("-" * table_width)

        for symbol in largest_symbols:
            # Use demangled name if available, otherwise raw name
            display_name = symbol.demangled or symbol.name
            name_display = display_name[:49] if len(display_name) > 49 else display_name
            lines.append(
                f"{name_display:<50} {symbol.sym_type:<6} {symbol.size:>8} B  {symbol.section}"
            )

        # Summary
        lines.append("")
        lines.append("=" * table_width)
        lines.append("SUMMARY")
        lines.append("=" * table_width)
        lines.append(f"Total strings found in RAM: {len(self.ram_strings)}")
        total_string_bytes = self.get_total_string_bytes()
        lines.append(f"Total bytes used by strings: {total_string_bytes:,}")

        # Optimization targets
        lines.append("")
        lines.append("=" * table_width)
        lines.append("POTENTIAL OPTIMIZATION TARGETS")
        lines.append("=" * table_width)

        # Repeated strings
        repeated = self.get_repeated_strings()[:10]
        if repeated:
            lines.append("\nRepeated strings (could be deduplicated):")
            for string, count in repeated:
                savings = (count - 1) * (len(string) + 1)
                clean_string = string[:50] + ("..." if len(string) > 50 else "")
                lines.append(
                    f'  "{clean_string}" - appears {count} times (potential savings: {savings} bytes)'
                )

        # Long strings - platform-specific advice
        long_strings = self.get_long_strings(20)[:10]
        if long_strings:
            if self.platform == "esp8266":
                lines.append(
                    "\nLong strings that could be moved to PROGMEM (>= 20 chars):"
                )
            else:
                # ESP32: strings in DRAM are typically there for a reason
                # (interrupt handlers, pre-flash-init code, etc.)
                lines.append("\nLong strings in DRAM (>= 20 chars):")
                lines.append(
                    "Note: ESP32 DRAM strings may be required for interrupt/early-boot contexts"
                )
            for ram_string in long_strings:
                clean_string = ram_string.content[:60] + (
                    "..." if len(ram_string.content) > 60 else ""
                )
                lines.append(
                    f'  {ram_string.section} @ 0x{ram_string.address:08x}: "{clean_string}" ({len(ram_string.content)} bytes)'
                )

        lines.append("")
        return "\n".join(lines)
