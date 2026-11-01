"""Tests for heuristic symbol pattern matching in the memory analyzer."""

import pytest

from esphome.analyze_memory import _split_patterns
from esphome.analyze_memory.const import CORE_SUBCATEGORY_PATTERNS

from . import make_analyzer


@pytest.mark.parametrize(
    "symbol",
    ["r_co_list_push_back", "r_rwip_reset", "rom_i2c_writeReg", "esp_rom_delay_us"],
)
def test_rom_symbols_are_rom_functions(symbol: str) -> None:
    assert make_analyzer()._identify_component(symbol) == "rom_functions"


@pytest.mark.parametrize(
    "symbol", ["r_ble_ll_conn_rx_data_pdu", "r_llc_hci_command_handler"]
)
def test_bluetooth_rom_symbols(symbol: str) -> None:
    assert make_analyzer()._identify_component(symbol) == "bluetooth_rom"


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
    assert make_analyzer()._identify_component(symbol) not in (
        "rom_functions",
        "bluetooth_rom",
    )


def test_split_patterns() -> None:
    assert _split_patterns({"a": ["^r_", "x"], "b": ["y"]}) == (
        ("a", ("x",), ("r_",)),
        ("b", ("y",), None),
    )


def test_prefix_marker_only_in_split_tables() -> None:
    # "^" is only honoured in SYMBOL_PATTERNS and DEMANGLED_PATTERNS
    assert not any(
        p.startswith("^") for ps in CORE_SUBCATEGORY_PATTERNS.values() for p in ps
    )
