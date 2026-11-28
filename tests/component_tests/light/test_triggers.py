"""Code generated for several on_turn_on and on_turn_off automations and an on_state one."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("triggers.yaml"))


def _body(main_cpp: str, function: str) -> str:
    """The body of the static function called ``function``."""
    match = re.search(
        rf"^static void {re.escape(function)}\(\) {{\n(.*?)^}}$",
        main_cpp,
        re.DOTALL | re.MULTILINE,
    )
    assert match is not None, function
    return match.group(1)


def _calls(body: str) -> list[str]:
    return re.findall(r"^\s*(\w+)\(\);$", body, re.MULTILINE)


def _function_filtering(main_cpp: str, check: str) -> list[str]:
    """The automation functions whose filter is ``check``, in the order they are defined."""
    return re.findall(
        rf"^static void (\w+)\(\) {{\n  if \(!\({re.escape(check)}\)\)\n    return;$",
        main_cpp,
        re.MULTILINE,
    )


def test_each_on_turn_on_has_its_own_flag_and_filter(main_cpp: str) -> None:
    for index in (0, 1):
        assert main_cpp.count(f"static bool lamp_on_turn_on_{index}_last;") == 1
        check = (
            f"light::update_turned_on(lamp_on_turn_on_{index}_last, "
            "::lamp->remote_values.is_on())"
        )
        assert len(_function_filtering(main_cpp, check)) == 1


def test_remote_values_callback_runs_the_automations_in_order(main_cpp: str) -> None:
    turn_on = [
        _function_filtering(
            main_cpp,
            f"light::update_turned_on(lamp_on_turn_on_{index}_last, "
            "::lamp->remote_values.is_on())",
        )[0]
        for index in (0, 1)
    ]
    calls = _calls(_body(main_cpp, "esphome__lamp__remote_values"))
    # Both on_turn_on in YAML order, then the on_state one, which has no filter
    assert calls[:2] == turn_on
    assert len(calls) == 3
    assert "state" in _body(main_cpp, calls[2])


def test_each_on_turn_off_tests_the_light_itself(main_cpp: str) -> None:
    turn_off = _function_filtering(main_cpp, "!::lamp->current_values.is_on()")
    assert len(turn_off) == 2
    assert _calls(_body(main_cpp, "esphome__lamp__target_state_reached")) == turn_off


def test_one_registration_per_callback_list(main_cpp: str) -> None:
    assert main_cpp.count("lamp->add_remote_values_callback(") == 1
    assert "lamp->add_remote_values_callback(esphome__lamp__remote_values);" in main_cpp
    assert main_cpp.count("lamp->add_target_state_reached_callback(") == 1
    assert (
        "lamp->add_target_state_reached_callback(esphome__lamp__target_state_reached);"
        in main_cpp
    )


def test_on_state_can_name_an_on_turn_off_automation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """on_turn_off is built before on_state, as before, so on_state can use its automation's ID."""
    main_cpp = generate_main(component_config_path("trigger_order.yaml"))
    assert "off_automation->stop();" in main_cpp
