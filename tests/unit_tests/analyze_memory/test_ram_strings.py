"""Tests for RAM symbol analysis in the RAM strings analyzer."""

from pathlib import Path
from unittest.mock import patch

from esphome.analyze_memory.ram_strings import RamStringsAnalyzer, SectionInfo

# nm -S --size-sort output with the newlib lock mutexes: nine global
# symbols that are all aliases of two local StaticSemaphore_t objects.
NM_OUTPUT_WITH_ALIASES = """\
3ffb4400 00000010 B small_symbol
3ffb43c8 00000054 B __lock___atexit_recursive_mutex
3ffb43c8 00000054 B __lock___env_recursive_mutex
3ffb43c8 00000054 B __lock___malloc_recursive_mutex
3ffb43c8 00000054 B __lock___sfp_recursive_mutex
3ffb43c8 00000054 B __lock___sinit_recursive_mutex
3ffb43c8 00000054 b s_common_recursive_mutex
3ffb441c 00000054 B __lock___arc4random_mutex
3ffb441c 00000054 B __lock___at_quick_exit_mutex
3ffb441c 00000054 B __lock___dd_hash_mutex
3ffb441c 00000054 B __lock___tz_mutex
3ffb441c 00000054 b s_common_mutex
"""


def _make_analyzer(tmp_path) -> RamStringsAnalyzer:
    """Create an analyzer with a dummy ELF and a .dram0.bss section."""
    elf = tmp_path / "firmware.elf"
    elf.write_bytes(b"\x7fELF")
    analyzer = RamStringsAnalyzer(str(elf), platform="esp32")
    analyzer.sections[".dram0.bss"] = SectionInfo(".dram0.bss", 0x3FFB0000, 0x10000)
    return analyzer


def _run_symbol_analysis(analyzer: RamStringsAnalyzer, nm_output: str) -> None:
    """Run _analyze_symbols with mocked nm output."""
    with (
        patch(
            "esphome.analyze_memory.ram_strings.find_tool",
            return_value="nm",
        ),
        patch.object(analyzer, "_run_command", return_value=nm_output),
    ):
        analyzer._analyze_symbols()


def test_aliased_symbols_counted_once(tmp_path: Path) -> None:
    """Symbols sharing an address are one object, not one per name."""
    analyzer = _make_analyzer(tmp_path)
    _run_symbol_analysis(analyzer, NM_OUTPUT_WITH_ALIASES)

    # Three distinct addresses, so three symbols
    assert len(analyzer.ram_symbols) == 3
    total = sum(s.size for s in analyzer.ram_symbols)
    assert total == 0x10 + 0x54 + 0x54


def test_aliases_recorded_on_first_symbol(tmp_path: Path) -> None:
    """Extra names at the same address are kept as aliases."""
    analyzer = _make_analyzer(tmp_path)
    _run_symbol_analysis(analyzer, NM_OUTPUT_WITH_ALIASES)

    by_addr = {s.address: s for s in analyzer.ram_symbols}
    assert len(by_addr[0x3FFB43C8].aliases) == 5
    assert len(by_addr[0x3FFB441C].aliases) == 4
    assert by_addr[0x3FFB4400].aliases == []
    assert "s_common_mutex" in by_addr[0x3FFB441C].aliases


def test_alias_count_shown_in_report(tmp_path: Path) -> None:
    """The large symbols table notes how many aliases were merged."""
    analyzer = _make_analyzer(tmp_path)
    _run_symbol_analysis(analyzer, NM_OUTPUT_WITH_ALIASES)

    report = analyzer.generate_report()
    assert "(+5 aliases)" in report
    assert "(+4 aliases)" in report
    # Each lock name appears at most once in the report
    assert report.count("__lock___") == 2


def test_global_name_preferred_over_local_alias(tmp_path: Path) -> None:
    """A global name becomes the primary even when nm lists a local first."""
    analyzer = _make_analyzer(tmp_path)
    nm_output = """\
3ffb43c8 00000054 b s_common_recursive_mutex
3ffb43c8 00000054 B __lock___atexit_recursive_mutex
3ffb43c8 00000054 B __lock___malloc_recursive_mutex
"""
    _run_symbol_analysis(analyzer, nm_output)

    (symbol,) = analyzer.ram_symbols
    assert symbol.name == "__lock___atexit_recursive_mutex"
    assert symbol.sym_type == "B"
    assert sorted(symbol.aliases) == [
        "__lock___malloc_recursive_mutex",
        "s_common_recursive_mutex",
    ]


def test_alias_note_survives_name_truncation(tmp_path: Path) -> None:
    """Long names are truncated but the alias note is kept intact."""
    analyzer = _make_analyzer(tmp_path)
    long_name = "a_very_long_symbol_name_that_exceeds_the_column_width_by_far"
    nm_output = f"""\
3ffb43c8 00000054 B {long_name}
3ffb43c8 00000054 B other_name
"""
    _run_symbol_analysis(analyzer, nm_output)

    report = analyzer.generate_report()
    row = next(line for line in report.splitlines() if "(+1 aliases)" in line)
    name_column = row[:50].rstrip()
    assert name_column.endswith("(+1 aliases)")
    assert name_column.startswith("a_very_long_symbol_name")


def test_symbols_outside_ram_sections_skipped(tmp_path: Path) -> None:
    """Symbols outside known RAM sections are ignored entirely."""
    analyzer = _make_analyzer(tmp_path)
    nm_output = "40080000 00000100 B not_in_ram\n"
    _run_symbol_analysis(analyzer, nm_output)
    assert analyzer.ram_symbols == []
