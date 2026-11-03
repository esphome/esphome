"""Codegen tests for the esp32_ble_tracker service/manufacturer data triggers."""

from collections.abc import Callable
from pathlib import Path

UUID128 = (
    "(uint8_t*)(const uint8_t[16]){0xCD,0xAB,0xCD,0xAB,"
    "0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB}"
)


def test_uuid_and_address_setters(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each UUID width picks its setter; only the filtered triggers get an address."""
    main_cpp = generate_main(component_config_path("advertise_triggers.yaml"))

    assert "set_service_uuid16(0xABCDULL)" in main_cpp
    assert "set_service_uuid32(0xABCDABCDULL)" in main_cpp
    assert f"set_service_uuid128({UUID128})" in main_cpp
    assert "set_manufacturer_uuid16(0xABCDULL)" in main_cpp
    assert "set_manufacturer_uuid32(0xABCDABCDULL)" in main_cpp
    assert f"set_manufacturer_uuid128({UUID128})" in main_cpp
    assert "set_address(0xAC3743775F4CULL)" in main_cpp
    assert "set_address(0x112233445566ULL)" in main_cpp
    assert main_cpp.count("->set_address(") == 2
