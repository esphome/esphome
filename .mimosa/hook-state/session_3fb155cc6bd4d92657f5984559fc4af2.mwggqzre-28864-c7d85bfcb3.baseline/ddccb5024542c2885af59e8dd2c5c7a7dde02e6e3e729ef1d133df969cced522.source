"""transmit_rc6 keeps a toggle bit per action, and transmit_pronto sends its string."""

from collections.abc import Callable
from pathlib import Path

from esphome import automation
import esphome.codegen as cg
from esphome.components.remote_base import _protocol_field
from esphome.const import CONF_TEMPERATURE
from tests.component_tests.remote_transmitter import protocol_call_bodies


def test_rc6_toggle_and_pronto(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("rc6_pronto.yaml"))
    bodies = protocol_call_bodies(main_cpp)
    assert len(bodies) == 3
    toggle = (
        "apply_call.data.toggle = []() { static uint8_t toggle; return toggle++; }();"
    )
    # Each RC6 action has its own generated function, so its own toggle static.
    assert toggle in bodies[0]
    assert toggle in bodies[1]
    assert (
        'apply_call.data.data = progmem_string(ESPHOME_F("0000 006D 0001 0000 0015 0016"));'
        in bodies[2]
    )
    assert "delta" not in bodies[2]


def test_apply_objects_pass_through() -> None:
    call = automation.ApplyCall("stop()")
    assert _protocol_field(call) is call
    field = automation.ApplyField(
        CONF_TEMPERATURE, "data.set_temperature({})", cg.float_
    )
    assert _protocol_field(field) is field
    plain = _protocol_field((CONF_TEMPERATURE, "temperature", cg.float_))
    assert plain.target == "data.temperature = {}"
