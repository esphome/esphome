"""Codegen tests for the esp32_ble_tracker service/manufacturer data triggers."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.helpers import ble_match_bodies, ble_match_body


def test_uuid_and_address_match_functions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each UUID width picks its constexpr factory; only filtered triggers test the address."""
    main_cpp = generate_main(component_config_path("advertise_triggers.yaml"))

    assert ble_match_bodies(main_cpp) == [
        ble_match_body("get_service_datas", "from_uint16(0xABCD)"),
        ble_match_body(
            "get_service_datas", "from_uint32(0xABCDABCD)", mac="0xAC3743775F4CULL"
        ),
        ble_match_body("get_service_datas", raw=True),
        ble_match_body(
            "get_manufacturer_datas", "from_uint16(0xABCD)", mac="0x112233445566ULL"
        ),
        ble_match_body("get_manufacturer_datas", "from_uint32(0xABCDABCD)"),
        ble_match_body("get_manufacturer_datas", raw=True),
    ]
