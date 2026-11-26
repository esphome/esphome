"""Code generated for one automation per change trigger key, and two on_speed_set ones."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest

_GROUP = "esphome__fan_a__on_state_callback"

# (last value static, its type, the filter, the x forwarded to the actions or None), in YAML order
_AUTOMATIONS = (
    (
        "fan_a_on_turn_on_0_last",
        "bool",
        "esphome::change_trigger::entered(fan_a_on_turn_on_0_last, ::fan_a->state)",
        None,
    ),
    (
        "fan_a_on_turn_off_0_last",
        "bool",
        "esphome::change_trigger::left(fan_a_on_turn_off_0_last, ::fan_a->state)",
        None,
    ),
    (
        "fan_a_on_direction_set_0_last",
        "fan::FanDirection",
        "esphome::change_trigger::changed(fan_a_on_direction_set_0_last, ::fan_a->direction)",
        "static_cast<fan::FanDirection>(::fan_a->direction)",
    ),
    (
        "fan_a_on_oscillating_set_0_last",
        "bool",
        "esphome::change_trigger::changed(fan_a_on_oscillating_set_0_last, ::fan_a->oscillating)",
        "static_cast<bool>(::fan_a->oscillating)",
    ),
    (
        "fan_a_on_speed_set_0_last",
        "int",
        "esphome::change_trigger::changed(fan_a_on_speed_set_0_last, ::fan_a->speed)",
        "static_cast<int>(::fan_a->speed)",
    ),
    (
        "fan_a_on_speed_set_1_last",
        "int",
        "esphome::change_trigger::changed(fan_a_on_speed_set_1_last, ::fan_a->speed)",
        "static_cast<int>(::fan_a->speed)",
    ),
    (
        "fan_a_on_preset_set_0_last",
        "const char *",
        (
            "esphome::change_trigger::changed(fan_a_on_preset_set_0_last,"
            " ::fan_a->get_preset_mode().c_str())"
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


def _runs(main_cpp: str) -> list[tuple[str, str]]:
    """Each (filter, automation function) of the group function, in order."""
    return re.findall(
        r"^  if \((.*)\)\n    (\w+)\(\);$", _body(main_cpp, _GROUP), re.MULTILINE
    )


def test_each_automation_has_its_own_last_value(main_cpp: str) -> None:
    body = _body(main_cpp, _GROUP)
    for last, type_, _, _ in _AUTOMATIONS:
        assert body.count(f"static {type_} {last};") == 1
    assert len(re.findall(r"^  static .* \w+_last;$", body, re.MULTILINE)) == len(
        _AUTOMATIONS
    )


def test_filters_run_the_automations_in_yaml_order(main_cpp: str) -> None:
    assert [check for check, _ in _runs(main_cpp)] == [
        check for _, _, check, _ in _AUTOMATIONS
    ]


def test_actions_get_a_copy_of_the_value(main_cpp: str) -> None:
    for (_, _, _, forward), (_, function) in zip(
        _AUTOMATIONS, _runs(main_cpp), strict=True
    ):
        if forward is None:
            continue
        call = _body(main_cpp, function).splitlines()[-1]
        assert call.endswith(f"({forward});"), call


def test_one_registration_on_the_state_callback(main_cpp: str) -> None:
    assert main_cpp.count("fan_a->add_on_state_callback(") == 1
    assert f"fan_a->add_on_state_callback({_GROUP});" in main_cpp
