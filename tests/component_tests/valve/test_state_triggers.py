"""Tests for the valve on_open and on_closed triggers."""

from collections.abc import Callable
from pathlib import Path


def test_state_triggers_fire_on_entering_their_end_position(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("state_triggers.yaml"))

    assert main_cpp.count("tap->add_on_state_callback(") == 1
    assert "tap->add_on_state_callback(esphome__tap__on_state_callback);" in main_cpp
    body = main_cpp.split("static void esphome__tap__on_state_callback() {")[1]
    body = body.split("\n}")[0]
    # Each keeps whether it was in its end position, so it fires once per arrival
    for key, check in (
        ("on_open", "::tap->is_fully_open()"),
        ("on_closed", "::tap->is_fully_closed()"),
    ):
        assert f"static bool tap_{key}_0_last;" in body
        assert (
            f"if (esphome::internal::change_trigger::entered(tap_{key}_0_last, {check}))"
            in body
        )
    assert body.index("is_fully_open()") < body.index("is_fully_closed()")
    assert "drain->add_on_state_callback(" not in main_cpp
