"""Tests for RAM symbol analysis in the RAM strings analyzer."""

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


def test_aliased_symbols_counted_once(tmp_path) -> None:
    """Symbols sharing an address are one object, not one per name."""
    analyzer = _make_analyzer(tmp_path)
    _run_symbol_analysis(analyzer, NM_OUTPUT_WITH_ALIASES)

    # Three distinct addresses, so three symbols
    assert len(analyzer.ram_symbols) == 3
    total = sum(s.size for s in analyzer.ram_symbols)
    assert total == 0x10 + 0x54 + 0x54


def test_aliases_recorded_on_first_symbol(tmp_path) -> None:
    """Extra names at the same address are kept as aliases."""
    analyzer = _make_analyzer(tmp_path)
    _run_symbol_analysis(analyzer, NM_OUTPUT_WITH_ALIASES)

    by_addr = {s.address: s for s in analyzer.ram_symbols}
    assert len(by_addr[0x3FFB43C8].aliases) == 5
    assert len(by_addr[0x3FFB441C].aliases) == 4
    assert by_addr[0x3FFB4400].aliases == []
    assert "s_common_mutex" in by_addr[0x3FFB441C].aliases


def test_alias_count_shown_in_report(tmp_path) -> None:
    """The large symbols table notes how many aliases were merged."""
    analyzer = _make_analyzer(tmp_path)
    _run_symbol_analysis(analyzer, NM_OUTPUT_WITH_ALIASES)

    report = analyzer.generate_report()
    assert "(+5 aliases)" in report
    assert "(+4 aliases)" in report
    # Each lock name appears at most once in the report
    assert report.count("__lock___") == 2


def test_symbols_outside_ram_sections_skipped(tmp_path) -> None:
    """Symbols outside known RAM sections are ignored entirely."""
    analyzer = _make_analyzer(tmp_path)
    nm_output = "40080000 00000100 B not_in_ram\n"
    _run_symbol_analysis(analyzer, nm_output)
    assert analyzer.ram_symbols == []
