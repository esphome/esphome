"""Tests for the code generated for interval blocks."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

CONFIG = "interval_codegen.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path(CONFIG))


def _constructed(main_cpp: str, var: str) -> str:
    match = re.search(rf"new\({var}\) ([^;]+);", main_cpp)
    assert match is not None, var
    return match.group(1)


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
    """Every interval gets its update interval; only a non-zero startup delay is built in."""
    for var, interval, delay in (
        ("interval_intervaltrigger_id", 1000, 0),
        ("interval_intervaltrigger_id_2", 2000, 5000),
        ("interval_intervaltrigger_id_3", 3000, 0),
        ("named_interval", 4000, 3000),
        ("interval_intervaltrigger_id_4", 5000, 0),
    ):
        assert f"{var}->set_update_interval({interval});" in main_cpp
        expected = (
            f"interval::DelayedIntervalTrigger({delay})"
            if delay
            else "interval::IntervalTrigger()"
        )
        assert _constructed(main_cpp, var) == expected
    assert "set_startup_delay" not in main_cpp


def test_interval_setup_priority_override(main_cpp: str) -> None:
    """A setup_priority set on an interval is applied before it is registered."""
    assert (
        "interval_intervaltrigger_id_4->set_setup_priority(250.0f);\n"
        "App.register_component_(interval_intervaltrigger_id_4, " in main_cpp
    )
    assert main_cpp.count("set_setup_priority") == 1


def test_interval_inline_actions(main_cpp: str) -> None:
    """All-inline intervals build an IntervalTrigger with an Automation of actions."""
    assert (
        _constructed(main_cpp, "interval_intervaltrigger_id")
        == "interval::IntervalTrigger()"
    )
    assert (
        _constructed(main_cpp, "interval_intervaltrigger_id_2")
        == "interval::DelayedIntervalTrigger(5000)"
    )
    assert "new(automation_id) Automation<>(interval_intervaltrigger_id);" in main_cpp
    assert "automation_id->add_action(lambdaaction_id);" in main_cpp
    assert (
        "new(automation_id_2) Automation<>(interval_intervaltrigger_id_2);" in main_cpp
    )


def test_interval_delay_keeps_automation(main_cpp: str) -> None:
    """A delay needs the Automation of separate actions."""
    assert (
        _constructed(main_cpp, "interval_intervaltrigger_id_3")
        == "interval::IntervalTrigger()"
    )
    assert (
        "new(automation_id_3) Automation<>(interval_intervaltrigger_id_3);" in main_cpp
    )
    assert "automation_id_3->add_action(delayaction_id);" in main_cpp


def test_interval_manual_id_keeps_trigger(main_cpp: str) -> None:
    """An id set by hand can be used from lambdas and component actions as its trigger."""
    assert (
        _constructed(main_cpp, "named_interval")
        == "interval::DelayedIntervalTrigger(3000)"
    )
    assert (
        "static interval::DelayedIntervalTrigger *const named_interval = " in main_cpp
    )
    assert "new(automation_id_4) Automation<>(named_interval);" in main_cpp


def test_interval_registered_before_its_actions_are_built(main_cpp: str) -> None:
    """An interval waiting for a later id keeps its place and still gets its interval."""
    assert (
        _constructed(main_cpp, "interval_intervaltrigger_id_4")
        == "interval::IntervalTrigger()"
    )
    registered = main_cpp.index("App.register_component_(interval_intervaltrigger_id_4")
    assert registered < main_cpp.index("new(later_script)")
    assert main_cpp.index("::later_script->execute();") > 0
    assert main_cpp.index(
        "interval_intervaltrigger_id_4->set_update_interval(5000);"
    ) > main_cpp.index("new(later_script)")


def test_interval_looping_count(main_cpp: str) -> None:
    """Intervals do not loop; the looping component count names their class."""
    assert "(3 * HasLoopOverride<interval::IntervalTrigger>::value)" in main_cpp
    assert "(2 * HasLoopOverride<interval::DelayedIntervalTrigger>::value)" in main_cpp
