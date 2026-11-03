"""transmit_keeloq sets the suffix only when the config names it."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.remote_transmitter import protocol_call_bodies


def test_keeloq_suffix_is_set_only_when_configured(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("keeloq_suffix.yaml"))
    bodies = protocol_call_bodies(main_cpp)
    assert len(bodies) == 4
    plain, constant, lambda_, group = bodies
    # The plain word fills only the HCS301 fields.
    assert "data.suffix" not in plain
    assert "set_group" not in plain
    assert "apply_call.data.suffix = 0x2A;" in constant
    assert "apply_call.data.suffix_bits = 6;" in constant
    # A lambda is called inline.
    assert "apply_call.data.suffix = static_cast<uint16_t>(0x2A);" in lambda_
    assert "apply_call.data.suffix_bits = static_cast<uint8_t>(6);" in lambda_
    # group fills vlow, repeat and the suffix on the device.
    assert "apply_call.data.set_group(0x0F);" in group
    # It comes after the default level, so the group's bit 0 is what is sent.
    assert group.index("data.vlow") < group.index("set_group")
    assert "data.suffix" not in group
