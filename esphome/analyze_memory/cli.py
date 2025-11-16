"""CLI interface for memory analysis with report generation."""

from collections import defaultdict
import sys

from . import (
    _COMPONENT_API,
    _COMPONENT_CORE,
    _COMPONENT_PREFIX_ESPHOME,
    _COMPONENT_PREFIX_EXTERNAL,
    MemoryAnalyzer,
)


class MemoryAnalyzerCLI(MemoryAnalyzer):
    """Memory analyzer with CLI-specific report generation."""

    # Symbol size threshold for detailed analysis
    SYMBOL_SIZE_THRESHOLD: int = (
        100  # Show symbols larger than this in detailed analysis
    )

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
        lines.append("=" * self.TABLE_WIDTH)

        # Add ESPHome core detailed analysis if there are core symbols
        if self._esphome_core_symbols:
            lines.append("")
            lines.append("=" * self.TABLE_WIDTH)
            lines.append(
                f"{_COMPONENT_CORE} Detailed Analysis".center(self.TABLE_WIDTH)
            )
            lines.append("=" * self.TABLE_WIDTH)
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
                lines.append(f"{i + 1}. {demangled} ({size:,} B)")

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
                lines.append("")
                lines.append("=" * self.TABLE_WIDTH)
                lines.append(f"{comp_name} Detailed Analysis".center(self.TABLE_WIDTH))
                lines.append("=" * self.TABLE_WIDTH)
                lines.append("")

                # Sort symbols by size
                sorted_symbols = sorted(comp_symbols, key=lambda x: x[2], reverse=True)

                lines.append(f"Total symbols: {len(sorted_symbols)}")
                lines.append(f"Total size: {comp_mem.flash_total:,} B")
                lines.append("")

                # Show all symbols above threshold for better visibility
                large_symbols = [
                    (sym, dem, size)
                    for sym, dem, size in sorted_symbols
                    if size > self.SYMBOL_SIZE_THRESHOLD
                ]

                lines.append(
                    f"{comp_name} Symbols > {self.SYMBOL_SIZE_THRESHOLD} B ({len(large_symbols)} symbols):"
                )
                for i, (symbol, demangled, size) in enumerate(large_symbols):
                    lines.append(f"{i + 1}. {demangled} ({size:,} B)")

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
