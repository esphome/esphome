"""transmit_ira211 sets the temperature through IRA211Data::set_temperature()."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.remote_transmitter import protocol_call_bodies


def test_ira211_temperature_is_a_statement_on_the_data(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("ira211_action.yaml"))
    constant, lambda_ = protocol_call_bodies(main_cpp)
    assert "apply_call.data.set_temperature(22.5f);" in constant
    assert (
        "apply_call.data.mode = remote_base::IRA211Mode::IRA211_MODE_TIMER;" in constant
    )
    assert "apply_call.data.fan = remote_base::IRA211Fan::IRA211_FAN_HIGH;" in constant
    assert "apply_call.data.set_temperature(static_cast<float>(21.0f));" in lambda_
