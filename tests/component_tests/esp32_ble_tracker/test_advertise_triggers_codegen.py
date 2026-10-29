"""Codegen tests for the esp32_ble_tracker service/manufacturer data triggers."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.helpers import ble_adv_match, ble_adv_matches


def test_uuid_and_address_match_constants(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each UUID width picks its constexpr factory; identical filters share one constant."""
    main_cpp = generate_main(component_config_path("advertise_triggers.yaml"))

    assert ble_adv_matches(main_cpp) == [
        ble_adv_match("from_uint16(0xABCD)"),
        ble_adv_match("from_uint16(0xABCD)"),
        ble_adv_match("from_uint32(0xABCDABCD)", mac="0xAC3743775F4CULL"),
        ble_adv_match(raw=True),
        ble_adv_match(
            "from_uint16(0xABCD)", mac="0x112233445566ULL", manufacturer=True
        ),
        ble_adv_match("from_uint32(0xABCDABCD)", manufacturer=True),
        ble_adv_match(raw=True, manufacturer=True),
    ]
    assert main_cpp.count("static constexpr ble_device_base::AdvDataMatch ") == 6
