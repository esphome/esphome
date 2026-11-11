"""Codegen tests for the service and manufacturer data advertise triggers.

The match constants come from shared ble_device_base codegen; the full width,
flag and sharing matrix lives here, the other trackers only check it ran.
"""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.helpers import (
    BLE_UUID128_FACTORY,
    ble_adv_match,
    ble_adv_matches,
)


def test_uuid_and_address_match_constants(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each UUID width picks its constexpr factory; identical filters share one constant."""
    main_cpp = generate_main(component_config_path("advertise_triggers.yaml"))

    assert ble_adv_matches(main_cpp) == [
        ble_adv_match("from_uint16(0xABCDULL)"),
        ble_adv_match("from_uint16(0xABCDULL)"),
        ble_adv_match("from_uint32(0xABCDABCDULL)", mac="0xAC3743775F4CULL"),
        ble_adv_match(BLE_UUID128_FACTORY),
        ble_adv_match(
            "from_uint16(0xABCDULL)", manufacturer=True, mac="0x112233445566ULL"
        ),
        ble_adv_match("from_uint32(0xABCDABCDULL)", manufacturer=True),
        ble_adv_match(BLE_UUID128_FACTORY, manufacturer=True),
    ]
    assert main_cpp.count("static constexpr ble_device_base::AdvDataMatch ") == 6
