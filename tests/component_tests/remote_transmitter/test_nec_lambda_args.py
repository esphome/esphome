"""How a transmit action's field lambdas get the script's arguments."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.remote_transmitter import protocol_call_bodies


def test_nec_field_lambdas_get_the_script_args(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("nec_lambda_args.yaml"))
    (body,) = protocol_call_bodies(main_cpp)
    assert (
        "static void esphome__applyaction_id__fn(const std::remove_cvref_t<int> & x) {"
        in main_cpp
    )
    assert (
        "apply_call.data.address = [](const std::remove_cvref_t<int> & x) -> uint16_t {"
        in body
    )
    assert "x *= 2; return x;" in body
    assert "}(x);" in body
    assert "apply_call.data.command = static_cast<uint16_t>(x *= 2);" in body
