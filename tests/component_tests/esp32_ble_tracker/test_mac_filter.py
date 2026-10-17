"""Tests for the esp32_ble_tracker on_ble_advertise MAC filter codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.ble_device_base.automation import MAC_FILTER_LIST


def test_mac_filters_share_one_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Identical MAC lists share one flash table ended by 0; no or an empty filter sets none."""
    main_cpp = generate_main(component_config_path("mac_filter.yaml"))

    assert (
        "static constexpr uint64_t ble_mac_filter[] PROGMEM = "
        "{0xAC3743775F4CULL, 0x112233445566ULL, 0};" in main_cpp
    )
    assert main_cpp.count("set_addresses(ble_mac_filter);") == 2
    assert main_cpp.count("set_addresses(") == 2


def test_zero_mac_is_rejected() -> None:
    """00:00:00:00:00:00 ends the flash table, so it cannot be a filter entry."""
    assert MAC_FILTER_LIST(["11:22:33:44:55:66"])[0].parts == (
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
    )
    with pytest.raises(cv.Invalid):
        MAC_FILTER_LIST(["00:00:00:00:00:00"])
