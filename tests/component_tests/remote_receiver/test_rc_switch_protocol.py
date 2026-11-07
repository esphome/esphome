"""RC switch protocols are read from flash tables instead of being copied per entity."""

from collections.abc import Callable
from pathlib import Path


def test_rc_switch_protocols_share_flash_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("rc_switch_protocol.yaml"))

    # One shared table for the identical custom protocols; the user id keeps its name.
    assert main_cpp.count("static constexpr remote_base::RCSwitchBase") == 1
    assert (
        "remote_base::RCSwitchBase rc_switch_custom_protocol_2[] PROGMEM = "
        "{remote_base::RCSwitchBase(320, 9920, 320, 960, 960, 320, false)};"
    ) in main_cpp
    assert main_cpp.count("rc_switch_protocol_copy(rc_switch_custom_protocol_2)") == 2
    # Receivers point into flash instead of holding a copy.
    assert "->set_protocol(&remote_base::RC_SWITCH_PROTOCOLS[1]);" in main_cpp
    assert "->set_protocol(rc_switch_custom_protocol_2);" in main_cpp
    # Numbered and lambda protocols keep their existing paths.
    assert "return remote_base::rc_switch_protocol(1);" in main_cpp
    assert "return remote_base::rc_switch_protocol(2);" in main_cpp
