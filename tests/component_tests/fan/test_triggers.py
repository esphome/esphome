"""Code generated for one automation per change trigger key, and two on_speed_set ones."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest

# (last value static, its type, the filter, the x forwarded to the actions or None), in YAML order
_AUTOMATIONS = (
    (
        "esphome__fan_a__on_turn_on_0__last",
        "bool",
        (
            "(fan::update_changed(esphome__fan_a__on_turn_on_0__last, ::fan_a->state)"
            " && ::fan_a->state)"
        ),
        None,
    ),
    (
        "esphome__fan_a__on_turn_off_0__last",
        "bool",
        (
            "(fan::update_changed(esphome__fan_a__on_turn_off_0__last, ::fan_a->state)"
            " && (!::fan_a->state))"
        ),
        None,
    ),
    (
        "esphome__fan_a__on_direction_set_0__last",
        "fan::FanDirection",
        "fan::update_changed(esphome__fan_a__on_direction_set_0__last, ::fan_a->direction)",
        "static_cast<fan::FanDirection>(::fan_a->direction)",
    ),
    (
        "esphome__fan_a__on_oscillating_set_0__last",
        "bool",
        "fan::update_changed(esphome__fan_a__on_oscillating_set_0__last, ::fan_a->oscillating)",
        "static_cast<bool>(::fan_a->oscillating)",
    ),
    (
        "esphome__fan_a__on_speed_set_0__last",
        "int",
        "fan::update_changed(esphome__fan_a__on_speed_set_0__last, ::fan_a->speed)",
        "static_cast<int>(::fan_a->speed)",
    ),
    (
        "esphome__fan_a__on_speed_set_1__last",
        "int",
        "fan::update_changed(esphome__fan_a__on_speed_set_1__last, ::fan_a->speed)",
        "static_cast<int>(::fan_a->speed)",
    ),
    (
        "esphome__fan_a__on_preset_set_0__last",
        "const char *",
        (
            "fan::update_preset_changed(esphome__fan_a__on_preset_set_0__last,"
            " ::fan_a->get_preset_mode())"
        ),
        "static_cast<StringRef>(::fan_a->get_preset_mode())",
    ),
)


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


def _function_filtering(main_cpp: str, check: str) -> str:
    """The one automation function whose filter is ``check``."""
    functions = re.findall(
        rf"^static void (\w+)\(\) {{\n  if \(!\({re.escape(check)}\)\)\n    return;$",
        main_cpp,
        re.MULTILINE,
    )
    assert len(functions) == 1, check
    return functions[0]


def test_each_automation_has_its_own_last_value(main_cpp: str) -> None:
    names = [last for last, _, _, _ in _AUTOMATIONS]
    assert len(set(names)) == len(names)
    for last, type_, _, _ in _AUTOMATIONS:
        assert main_cpp.count(f"static {type_} {last};") == 1
    assert len(
        re.findall(r"^static .* esphome__fan_a__\w+__last;$", main_cpp, re.MULTILINE)
    ) == (len(_AUTOMATIONS))


def test_each_filter_watches_its_own_field(main_cpp: str) -> None:
    for _, _, check, _ in _AUTOMATIONS:
        _function_filtering(main_cpp, check)


def test_actions_get_a_copy_of_the_value(main_cpp: str) -> None:
    for _, _, check, forward in _AUTOMATIONS:
        body = _body(main_cpp, _function_filtering(main_cpp, check))
        call = body.splitlines()[-1]
        if forward is None:
            assert call.endswith("();"), call
        else:
            assert call.endswith(f"({forward});"), call


def test_change_function_runs_the_automations_in_yaml_order(main_cpp: str) -> None:
    expected = [_function_filtering(main_cpp, check) for _, _, check, _ in _AUTOMATIONS]
    calls = re.findall(
        r"^\s*(\w+)\(\);$", _body(main_cpp, "esphome__fan_a__change_fn"), re.MULTILINE
    )
    assert calls == expected


def test_one_registration_on_the_state_callback(main_cpp: str) -> None:
    assert main_cpp.count("fan_a->add_on_state_callback(") == 1
    assert "fan_a->add_on_state_callback(esphome__fan_a__change_fn);" in main_cpp
