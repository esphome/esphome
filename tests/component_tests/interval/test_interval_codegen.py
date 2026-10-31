"""Tests for the code generated for interval blocks."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from tests.component_tests.helpers import constructed, function_body

CONFIG = "interval_codegen.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path(CONFIG))


def _callback(var: str, startup_delay: int = 0) -> str:
    if startup_delay:
        return (
            f"interval::DelayedIntervalCallback<esphome__{var}__fn, {startup_delay}>()"
        )
    return f"interval::IntervalCallback<esphome__{var}__fn>()"


def _registered(main_cpp: str) -> list[tuple[str, str]]:
    return re.findall(r"App\.register_component_\((\w+), (\d+)\);", main_cpp)


def test_interval_registration_order_and_source(main_cpp: str) -> None:
    """Each interval is one component, registered in config order under the interval source."""
    registered = _registered(main_cpp)
    assert [var for var, _ in registered[-6:]] == [
        "interval_intervaltrigger_id",
        "interval_intervaltrigger_id_2",
        "interval_intervaltrigger_id_3",
        "named_interval",
        "interval_intervaltrigger_id_4",
        "some_sensor",
    ]
    source = re.search(r'COMP_SRC_TABLE_STR_(\d+)\[\] PROGMEM = "interval";', main_cpp)
    assert source is not None
    assert {index for _, index in registered[-6:-1]} == {str(int(source.group(1)) + 1)}


def test_interval_update_interval_and_startup_delay(main_cpp: str) -> None:
    """Every interval gets its update interval; a non-zero startup delay is a template argument.

    Only the IntervalTrigger of a hand-set id takes it at run time.
    """
    for var, interval in (
        ("interval_intervaltrigger_id", 1000),
        ("interval_intervaltrigger_id_2", 2000),
        ("interval_intervaltrigger_id_3", 3000),
        ("named_interval", 4000),
        ("interval_intervaltrigger_id_4", 5000),
    ):
        assert f"{var}->set_update_interval({interval});" in main_cpp
    assert constructed(main_cpp, "interval_intervaltrigger_id_2") == _callback(
        "interval_intervaltrigger_id_2", 5000
    )
    assert "named_interval->set_startup_delay(3000);" in main_cpp
    assert main_cpp.count("set_startup_delay") == 1


def test_interval_setup_priority_override(main_cpp: str) -> None:
    """A setup_priority set on an interval is applied before it is registered."""
    assert (
        "interval_intervaltrigger_id_4->set_setup_priority(250.0f);\n"
        "App.register_component_(interval_intervaltrigger_id_4, " in main_cpp
    )
    assert main_cpp.count("set_setup_priority") == 1


def test_interval_inline_actions(main_cpp: str) -> None:
    """All-inline intervals call one function from update(), with no Automation."""
    assert constructed(main_cpp, "interval_intervaltrigger_id") == _callback(
        "interval_intervaltrigger_id"
    )
    assert "Automation<>(interval_intervaltrigger_id" not in main_cpp
    # Both actions run in the one function, with no stop checks
    body = function_body(main_cpp, "esphome__interval_intervaltrigger_id__fn")
    assert "inline tick" in body
    assert "inline lambda" in body
    assert "esphome__running" not in body
    assert "Automation" not in body
    assert "delayed start tick" in function_body(
        main_cpp, "esphome__interval_intervaltrigger_id_2__fn"
    )


def test_interval_delay_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation of separate actions, which the function triggers."""
    assert constructed(main_cpp, "interval_intervaltrigger_id_3") == _callback(
        "interval_intervaltrigger_id_3"
    )
    assert "new(automation_id_3) Automation<>();" in main_cpp
    assert "automation_id_3->add_action(delayaction_id);" in main_cpp
    assert function_body(main_cpp, "esphome__interval_intervaltrigger_id_3__fn") == (
        "\n  ::automation_id_3->trigger();"
    )


def test_interval_manual_id_keeps_trigger(main_cpp: str) -> None:
    """An id set by hand can be used from lambdas and component actions as an IntervalTrigger."""
    assert constructed(main_cpp, "named_interval") == "interval::IntervalTrigger()"
    assert "static interval::IntervalTrigger *const named_interval = " in main_cpp
    assert "new(automation_id_4) Automation<>(named_interval);" in main_cpp


def test_interval_registered_before_its_actions_are_built(main_cpp: str) -> None:
    """An interval waiting for a later id keeps its place and still gets its interval."""
    assert constructed(main_cpp, "interval_intervaltrigger_id_4") == _callback(
        "interval_intervaltrigger_id_4"
    )
    registered = main_cpp.index("App.register_component_(interval_intervaltrigger_id_4")
    assert registered < main_cpp.index("new(later_script)")
    assert "::later_script->execute();" in function_body(
        main_cpp, "esphome__interval_intervaltrigger_id_4__fn"
    )
    assert main_cpp.index(
        "interval_intervaltrigger_id_4->set_update_interval(5000);"
    ) > main_cpp.index("new(later_script)")


def test_interval_looping_count(main_cpp: str) -> None:
    """Intervals do not loop; the looping component count names each class."""
    assert (
        "(1 * HasLoopOverride<interval::IntervalCallback<"
        "esphome__interval_intervaltrigger_id__fn>>::value)" in main_cpp
    )
    assert "(1 * HasLoopOverride<interval::IntervalTrigger>::value)" in main_cpp
