"""Tests for heuristic symbol pattern matching in the memory analyzer."""

from unittest.mock import patch

import pytest

from esphome.analyze_memory import MemoryAnalyzer


def _make_analyzer() -> MemoryAnalyzer:
    """Create a MemoryAnalyzer with no library or source attribution."""
    with patch.object(MemoryAnalyzer, "__init__", lambda self, *a, **kw: None):
        analyzer = MemoryAnalyzer.__new__(MemoryAnalyzer)
        analyzer.external_components = set()
        analyzer._demangle_cache = {}
        analyzer._lib_symbol_map = {}
        analyzer._source_symbol_map = {}
        analyzer._heuristic_to_lib = {}
    return analyzer


@pytest.mark.parametrize(
    "symbol",
    ["r_co_list_push_back", "r_rwip_reset", "rom_i2c_writeReg", "esp_rom_delay_us"],
)
def test_rom_symbols_are_rom_functions(symbol: str) -> None:
    assert _make_analyzer()._identify_component(symbol) == "rom_functions"


@pytest.mark.parametrize(
    "symbol", ["r_ble_ll_conn_rx_data_pdu", "r_llc_hci_command_handler"]
)
def test_bluetooth_rom_symbols(symbol: str) -> None:
    assert _make_analyzer()._identify_component(symbol) == "bluetooth_rom"


@pytest.mark.parametrize(
    "symbol",
    [
        "esphome__sensor_deltafilter_id__fn",
        "_esp_error_check_failed",
        "esp_set_time_from_rtc",
        "reserved_region_rom_data",
        "scanner_ble_gap_event",
        "timer_llc_poll",
    ],
)
def test_r_or_rom_inside_a_name_is_not_rom(symbol: str) -> None:
    assert _make_analyzer()._identify_component(symbol) not in (
        "rom_functions",
        "bluetooth_rom",
    )
