"""Tests for the esp32_ble_tracker on_ble_advertise MAC filter codegen."""

from collections.abc import Callable
from pathlib import Path


def test_mac_filters_share_one_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Identical MAC lists share one flash table ended by 0; no filter sets none."""
    main_cpp = generate_main(component_config_path("mac_filter.yaml"))

    assert (
        "static constexpr uint64_t ble_mac_filter[] PROGMEM = "
        "{0xAC3743775F4CULL, 0x112233445566ULL, 0};" in main_cpp
    )
    assert main_cpp.count("set_addresses(ble_mac_filter);") == 2
    assert main_cpp.count("set_addresses(") == 2
