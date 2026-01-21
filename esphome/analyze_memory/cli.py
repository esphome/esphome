"""CLI interface for memory analysis with report generation."""

from __future__ import annotations

from collections import defaultdict
from collections.abc import Callable
import sys
from typing import TYPE_CHECKING

from . import (
    _COMPONENT_API,
    _COMPONENT_CORE,
    _COMPONENT_PREFIX_ESPHOME,
    _COMPONENT_PREFIX_EXTERNAL,
    RAM_SECTIONS,
    MemoryAnalyzer,
)

if TYPE_CHECKING:
    from . import ComponentMemory


class MemoryAnalyzerCLI(MemoryAnalyzer):
    """Memory analyzer with CLI-specific report generation."""

    # Symbol size threshold for detailed analysis
    SYMBOL_SIZE_THRESHOLD: int = (
        100  # Show symbols larger than this in detailed analysis
    )
    # Lower threshold for RAM symbols (RAM is more constrained)
    RAM_SYMBOL_SIZE_THRESHOLD: int = 24

    # Column width constants
    COL_COMPONENT: int = 29
    COL_FLASH_TEXT: int = 14
    COL_FLASH_DATA: int = 14
    COL_RAM_DATA: int = 12
    COL_RAM_BSS: int = 12
    COL_TOTAL_FLASH: int = 15
    COL_TOTAL_RAM: int = 12
    COL_SEPARATOR: int = 3  # " | "

    # Core analysis column widths
    COL_CORE_SUBCATEGORY: int = 30
    COL_CORE_SIZE: int = 12
    COL_CORE_COUNT: int = 6
    COL_CORE_PERCENT: int = 10

    # Calculate table width once at class level
    TABLE_WIDTH: int = (
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

    @staticmethod
    def _make_separator_line(*widths: int) -> str:
        """Create a separator line with given column widths.

        Args:
            widths: Column widths to create separators for

        Returns:
            Separator line like "----+---------+-----"
        """
        return "-+-".join("-" * width for width in widths)

    # Pre-computed separator lines
    MAIN_TABLE_SEPARATOR: str = _make_separator_line(
        COL_COMPONENT,
        COL_FLASH_TEXT,
        COL_FLASH_DATA,
        COL_RAM_DATA,
        COL_RAM_BSS,
        COL_TOTAL_FLASH,
        COL_TOTAL_RAM,
    )

    CORE_TABLE_SEPARATOR: str = _make_separator_line(
        COL_CORE_SUBCATEGORY,
        COL_CORE_SIZE,
        COL_CORE_COUNT,
        COL_CORE_PERCENT,
    )

    def _add_section_header(self, lines: list[str], title: str) -> None:
        """Add a section header with title centered between separator lines."""
        lines.append("")
        lines.append("=" * self.TABLE_WIDTH)
        lines.append(title.center(self.TABLE_WIDTH))
        lines.append("=" * self.TABLE_WIDTH)
        lines.append("")

    def _add_top_consumers(
        self,
        lines: list[str],
        title: str,
        components: list[tuple[str, ComponentMemory]],
        get_size: Callable[[ComponentMemory], int],
        total: int,
        memory_type: str,
        limit: int = 25,
    ) -> None:
        """Add a formatted list of top memory consumers to the report.

        Args:
            lines: List of report lines to append the output to.
            title: Section title to print before the list.
            components: Sequence of (name, ComponentMemory) tuples to analyze.
            get_size: Callable that takes a ComponentMemory and returns the
                size in bytes to use for ranking and display.
            total: Total size in bytes for computing percentage usage.
            memory_type: Label for the memory region (e.g., "flash" or "RAM").
            limit: Maximum number of components to include in the list.
        """
        lines.append("")
        lines.append(f"{title}:")
        for i, (name, mem) in enumerate(components[:limit]):
            size = get_size(mem)
            if size > 0:
                percentage = (size / total * 100) if total > 0 else 0
                lines.append(
                    f"{i + 1}. {name} ({size:,} B) - {percentage:.1f}% of analyzed {memory_type}"
                )

    def _format_symbol_with_section(
        self, demangled: str, size: int, section: str | None = None
    ) -> str:
        """Format a symbol entry, optionally adding a RAM section label.

        If section is one of the RAM sections (.data or .bss), a label like
        " [data]" or " [bss]" is appended. For non-RAM sections or when
        section is None, no section label is added.
        """
        section_label = ""
        if section in RAM_SECTIONS:
            section_label = f" [{section[1:]}]"  # .data -> [data], .bss -> [bss]
        return f"{demangled} ({size:,} B){section_label}"

    def generate_report(self, detailed: bool = False) -> str:
        """Generate a formatted memory report."""
        components = sorted(
            self.components.items(), key=lambda x: x[1].flash_total, reverse=True
        )

        # Calculate totals
        total_flash = sum(c.flash_total for _, c in components)
        total_ram = sum(c.ram_total for _, c in components)

        # Build report
        lines: list[str] = []

        lines.append("=" * self.TABLE_WIDTH)
        lines.append("Component Memory Analysis".center(self.TABLE_WIDTH))
        lines.append("=" * self.TABLE_WIDTH)
        lines.append("")

        # Main table - fixed column widths
        lines.append(
            f"{'Component':<{self.COL_COMPONENT}} | {'Flash (text)':>{self.COL_FLASH_TEXT}} | {'Flash (data)':>{self.COL_FLASH_DATA}} | {'RAM (data)':>{self.COL_RAM_DATA}} | {'RAM (bss)':>{self.COL_RAM_BSS}} | {'Total Flash':>{self.COL_TOTAL_FLASH}} | {'Total RAM':>{self.COL_TOTAL_RAM}}"
        )
        lines.append(self.MAIN_TABLE_SEPARATOR)

        for name, mem in components:
            if mem.flash_total > 0 or mem.ram_total > 0:
                flash_rodata = mem.rodata_size + mem.data_size
                lines.append(
                    f"{name:<{self.COL_COMPONENT}} | {mem.text_size:>{self.COL_FLASH_TEXT - 2},} B | {flash_rodata:>{self.COL_FLASH_DATA - 2},} B | "
                    f"{mem.data_size:>{self.COL_RAM_DATA - 2},} B | {mem.bss_size:>{self.COL_RAM_BSS - 2},} B | "
                    f"{mem.flash_total:>{self.COL_TOTAL_FLASH - 2},} B | {mem.ram_total:>{self.COL_TOTAL_RAM - 2},} B"
                )

        lines.append(self.MAIN_TABLE_SEPARATOR)
        lines.append(
            f"{'TOTAL':<{self.COL_COMPONENT}} | {' ':>{self.COL_FLASH_TEXT}} | {' ':>{self.COL_FLASH_DATA}} | "
            f"{' ':>{self.COL_RAM_DATA}} | {' ':>{self.COL_RAM_BSS}} | "
            f"{total_flash:>{self.COL_TOTAL_FLASH - 2},} B | {total_ram:>{self.COL_TOTAL_RAM - 2},} B"
        )

        # Show unattributed RAM (SDK/framework overhead)
        unattributed_bss, unattributed_data, unattributed_total = (
            self.get_unattributed_ram()
        )
        if unattributed_total > 0:
            lines.append("")
            lines.append(
                f"Unattributed RAM: {unattributed_total:,} B (SDK/framework overhead)"
            )
            if unattributed_bss > 0 and unattributed_data > 0:
                lines.append(
                    f"  .bss: {unattributed_bss:,} B | .data: {unattributed_data:,} B"
                )

            # Show SDK symbol breakdown if available
            sdk_by_lib = self.get_sdk_ram_by_library()
            if sdk_by_lib:
                lines.append("")
                lines.append("SDK library breakdown (static symbols not in ELF):")
                # Sort libraries by total size
                lib_totals = [
                    (lib, sum(s.size for s in syms), syms)
                    for lib, syms in sdk_by_lib.items()
                ]
                lib_totals.sort(key=lambda x: x[1], reverse=True)

                for lib_name, lib_total, syms in lib_totals:
                    if lib_total == 0:
                        continue
                    lines.append(f"  {lib_name}: {lib_total:,} B")
                    # Show top symbols from this library
                    for sym in sorted(syms, key=lambda s: s.size, reverse=True)[:3]:
                        section_label = sym.section.lstrip(".")
                        # Use demangled name (falls back to original if not demangled)
                        display_name = sym.demangled or sym.name
                        if len(display_name) > 50:
                            display_name = f"{display_name[:47]}..."
                        lines.append(
                            f"    {sym.size:>6,} B [{section_label}] {display_name}"
                        )

        # Top consumers
        self._add_top_consumers(
            lines,
            "Top Flash Consumers",
            components,
            lambda m: m.flash_total,
            total_flash,
            "flash",
        )

        ram_components = sorted(components, key=lambda x: x[1].ram_total, reverse=True)
        self._add_top_consumers(
            lines,
            "Top RAM Consumers",
            ram_components,
            lambda m: m.ram_total,
            total_ram,
            "RAM",
        )

        # Add ESPHome core detailed analysis if there are core symbols
        if self._esphome_core_symbols:
            self._add_section_header(lines, f"{_COMPONENT_CORE} Detailed Analysis")

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
                f"{'Subcategory':<{self.COL_CORE_SUBCATEGORY}} | {'Size':>{self.COL_CORE_SIZE}} | "
                f"{'Count':>{self.COL_CORE_COUNT}} | {'% of Core':>{self.COL_CORE_PERCENT}}"
            )
            lines.append(self.CORE_TABLE_SEPARATOR)

            core_total = sum(size for _, _, size in self._esphome_core_symbols)

            for subcategory, symbols, total_size in sorted_subcategories:
                percentage = (total_size / core_total * 100) if core_total > 0 else 0
                lines.append(
                    f"{subcategory:<{self.COL_CORE_SUBCATEGORY}} | {total_size:>{self.COL_CORE_SIZE - 2},} B | "
                    f"{len(symbols):>{self.COL_CORE_COUNT}} | {percentage:>{self.COL_CORE_PERCENT - 1}.1f}%"
                )

            # All core symbols above threshold
            lines.append("")
            sorted_core_symbols = sorted(
                self._esphome_core_symbols, key=lambda x: x[2], reverse=True
            )
            large_core_symbols = [
                (symbol, demangled, size)
                for symbol, demangled, size in sorted_core_symbols
                if size > self.SYMBOL_SIZE_THRESHOLD
            ]

            lines.append(
                f"{_COMPONENT_CORE} Symbols > {self.SYMBOL_SIZE_THRESHOLD} B ({len(large_core_symbols)} symbols):"
            )
            for i, (symbol, demangled, size) in enumerate(large_core_symbols):
                # Core symbols only track (symbol, demangled, size) without section info,
                # so we don't show section labels here
                lines.append(
                    f"{i + 1}. {self._format_symbol_with_section(demangled, size)}"
                )

            lines.append("=" * self.TABLE_WIDTH)

        # Add detailed analysis for top ESPHome and external components
        esphome_components = [
            (name, mem)
            for name, mem in components
            if name.startswith(_COMPONENT_PREFIX_ESPHOME) and name != _COMPONENT_CORE
        ]
        external_components = [
            (name, mem)
            for name, mem in components
            if name.startswith(_COMPONENT_PREFIX_EXTERNAL)
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
            if name == _COMPONENT_API:
                api_component = (name, mem)
                break

        # Also include wifi_stack and other important system components if they exist
        system_components_to_include = [
            # Empty list - we've finished debugging symbol categorization
            # Add component names here if you need to debug their symbols
        ]
        system_components = [
            (name, mem)
            for name, mem in components
            if name in system_components_to_include
        ]

        # Combine all components to analyze: top ESPHome + all external + API if not already included + system components
        components_to_analyze = (
            list(top_esphome_components)
            + list(top_external_components)
            + system_components
        )
        if api_component and api_component not in components_to_analyze:
            components_to_analyze.append(api_component)

        if components_to_analyze:
            for comp_name, comp_mem in components_to_analyze:
                if not (comp_symbols := self._component_symbols.get(comp_name, [])):
                    continue
                self._add_section_header(lines, f"{comp_name} Detailed Analysis")

                # Sort symbols by size
                sorted_symbols = sorted(comp_symbols, key=lambda x: x[2], reverse=True)

                lines.append(f"Total symbols: {len(sorted_symbols)}")
                lines.append(f"Total size: {comp_mem.flash_total:,} B")
                lines.append("")

                # Show all symbols above threshold for better visibility
                large_symbols = [
                    (sym, dem, size, sec)
                    for sym, dem, size, sec in sorted_symbols
                    if size > self.SYMBOL_SIZE_THRESHOLD
                ]

                lines.append(
                    f"{comp_name} Symbols > {self.SYMBOL_SIZE_THRESHOLD} B ({len(large_symbols)} symbols):"
                )
                for i, (symbol, demangled, size, section) in enumerate(large_symbols):
                    lines.append(
                        f"{i + 1}. {self._format_symbol_with_section(demangled, size, section)}"
                    )

                lines.append("=" * self.TABLE_WIDTH)

        # Detailed RAM analysis by component (at end, before RAM strings analysis)
        self._add_section_header(lines, "RAM Symbol Analysis by Component")

        # Show top 15 RAM consumers with their large symbols
        for name, mem in ram_components[:15]:
            if mem.ram_total == 0:
                continue
            ram_syms = self._ram_symbols.get(name, [])
            if not ram_syms:
                continue

            # Sort by size descending
            sorted_ram_syms = sorted(ram_syms, key=lambda x: x[2], reverse=True)
            large_ram_syms = [
                s for s in sorted_ram_syms if s[2] > self.RAM_SYMBOL_SIZE_THRESHOLD
            ]

            lines.append(f"{name} ({mem.ram_total:,} B total RAM):")

            # Show breakdown by section type
            data_size = sum(s[2] for s in ram_syms if s[3] == ".data")
            bss_size = sum(s[2] for s in ram_syms if s[3] == ".bss")
            lines.append(f"  .data (initialized): {data_size:,} B")
            lines.append(f"  .bss (uninitialized): {bss_size:,} B")

            if large_ram_syms:
                lines.append(
                    f"  Symbols > {self.RAM_SYMBOL_SIZE_THRESHOLD} B ({len(large_ram_syms)}):"
                )
                for symbol, demangled, size, section in large_ram_syms[:10]:
                    # Format section label consistently by stripping leading dot
                    section_label = section.lstrip(".") if section else ""
                    # Add ellipsis if name is truncated
                    demangled_display = (
                        f"{demangled[:70]}..." if len(demangled) > 70 else demangled
                    )
                    lines.append(
                        f"    {size:>6,} B [{section_label}] {demangled_display}"
                    )
                if len(large_ram_syms) > 10:
                    lines.append(f"    ... and {len(large_ram_syms) - 10} more")
            lines.append("")

        lines.append(
            "Note: This analysis covers symbols in the ELF file. Some runtime allocations may not be included."
        )
        lines.append("=" * self.TABLE_WIDTH)

        return "\n".join(lines)

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
            demangled_display = (
                demangled[:100] if symbol != demangled else "[not demangled]"
            )
            lines.append(f"{size:>10,} | {symbol[:60]:<60} | {demangled_display}")

        if len(sorted_symbols) > 100:
            lines.append(f"\n... and {len(sorted_symbols) - 100} more symbols")

        content = "\n".join(lines)

        if output_file:
            with open(output_file, "w", encoding="utf-8") as f:
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
    analyzer = MemoryAnalyzerCLI(
        elf_path, objdump_path, readelf_path, external_components
    )
    analyzer.analyze()
    return analyzer.generate_report(detailed)


def main():
    """CLI entrypoint for memory analysis."""
    if len(sys.argv) < 2:
        print("Usage: python -m esphome.analyze_memory <build_directory>")
        print("\nAnalyze memory usage from an ESPHome build directory.")
        print("The build directory should contain firmware.elf and idedata will be")
        print("loaded from ~/.esphome/.internal/idedata/<device>.json")
        print("\nExamples:")
        print("  python -m esphome.analyze_memory ~/.esphome/build/my-device")
        print("  python -m esphome.analyze_memory .esphome/build/my-device")
        print("  python -m esphome.analyze_memory my-device  # Short form")
        sys.exit(1)

    build_dir = sys.argv[1]

    # Load build directory
    import json
    from pathlib import Path

    from esphome.platformio_api import IDEData

    build_path = Path(build_dir)

    # If no path separator in name, assume it's a device name
    if "/" not in build_dir and not build_path.is_dir():
        # Try current directory first
        cwd_path = Path.cwd() / ".esphome" / "build" / build_dir
        if cwd_path.is_dir():
            build_path = cwd_path
            print(f"Using build directory: {build_path}", file=sys.stderr)
        else:
            # Fall back to home directory
            build_path = Path.home() / ".esphome" / "build" / build_dir
            print(f"Using build directory: {build_path}", file=sys.stderr)

    if not build_path.is_dir():
        print(f"Error: {build_path} is not a directory", file=sys.stderr)
        sys.exit(1)

    # Find firmware.elf
    elf_file = None
    for elf_candidate in [
        build_path / "firmware.elf",
        build_path / ".pioenvs" / build_path.name / "firmware.elf",
    ]:
        if elf_candidate.exists():
            elf_file = str(elf_candidate)
            break

    if not elf_file:
        print(f"Error: firmware.elf not found in {build_dir}", file=sys.stderr)
        sys.exit(1)

    # Find idedata.json - check current directory first, then home
    device_name = build_path.name
    idedata_candidates = [
        Path.cwd() / ".esphome" / "idedata" / f"{device_name}.json",
        Path.home() / ".esphome" / "idedata" / f"{device_name}.json",
    ]

    idedata = None
    for idedata_path in idedata_candidates:
        if not idedata_path.exists():
            continue
        try:
            with open(idedata_path, encoding="utf-8") as f:
                raw_data = json.load(f)
            idedata = IDEData(raw_data)
            print(f"Loaded idedata from: {idedata_path}", file=sys.stderr)
            break
        except (json.JSONDecodeError, OSError) as e:
            print(f"Warning: Failed to load idedata: {e}", file=sys.stderr)

    if not idedata:
        print(
            f"Warning: idedata not found (searched {idedata_candidates[0]} and {idedata_candidates[1]})",
            file=sys.stderr,
        )

    analyzer = MemoryAnalyzerCLI(elf_file, idedata=idedata)
    analyzer.analyze()
    report = analyzer.generate_report()
    print(report)


if __name__ == "__main__":
    main()
