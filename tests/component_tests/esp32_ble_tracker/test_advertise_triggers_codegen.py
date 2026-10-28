"""Codegen tests for the esp32_ble_tracker service/manufacturer data triggers."""

from collections.abc import Callable
from pathlib import Path

UUID128 = (
    "static constexpr uint8_t UUID[16] = {0xCD, 0xAB, 0xCD, 0xAB, 0xCD, 0xAB, "
    "0xCD, 0xAB, 0xCD, 0xAB, 0xCD, 0xAB, 0xCD, 0xAB, 0xCD, 0xAB}; "
)


def _find(getter: str, uuid: str) -> str:
    return f"return ble_device_base::find_adv_data(device.{getter}(), ble_device_base::ESPBTUUID::{uuid});"


def test_uuid_and_address_match_functions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each UUID width picks its factory; only the filtered triggers test the address."""
    main_cpp = generate_main(component_config_path("advertise_triggers.yaml"))

    assert main_cpp.count("ble_device_base::BLEAdvertiseDataTrigger(") == 6
    assert "{ " + _find("get_service_datas", "from_uint16(0xABCD)") in main_cpp
    assert (
        "{ if (device.address_uint64() != 0xAC3743775F4CULL) return nullptr; "
        + _find("get_service_datas", "from_uint32(0xABCDABCD)")
        in main_cpp
    )
    assert UUID128 + _find("get_service_datas", "from_raw(UUID)") in main_cpp
    assert (
        "{ if (device.address_uint64() != 0x112233445566ULL) return nullptr; "
        + _find("get_manufacturer_datas", "from_uint16(0xABCD)")
        in main_cpp
    )
    assert "{ " + _find("get_manufacturer_datas", "from_uint32(0xABCDABCD)") in main_cpp
    assert UUID128 + _find("get_manufacturer_datas", "from_raw(UUID)") in main_cpp
    assert main_cpp.count("device.address_uint64()") == 2
