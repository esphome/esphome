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


@pytest.mark.parametrize(
    ("var", "expected", "update_interval"),
    [
        ("interval_intervaltrigger_id", "interval::IntervalTrigger()", 1000),
        (
            "interval_intervaltrigger_id_2",
            "interval::DelayedIntervalTrigger(5000)",
            2000,
        ),
        ("interval_intervaltrigger_id_3", "interval::IntervalTrigger()", 3000),
        ("named_interval", "interval::DelayedIntervalTrigger(3000)", 4000),
        ("interval_intervaltrigger_id_4", "interval::IntervalTrigger()", 5000),
    ],
)
def test_interval_constructed(
    main_cpp: str, var: str, expected: str, update_interval: int
) -> None:
    """Each interval is built with its startup delay, if any, and gets its update interval."""
    assert _constructed(main_cpp, var) == expected
    assert f"{var}->set_update_interval({update_interval});" in main_cpp


def test_interval_delay_keeps_automation(main_cpp: str) -> None:
    """A delay needs the Automation of separate actions."""
    automation = re.search(
        r"new\((\w+)\) Automation<>\(interval_intervaltrigger_id_3\);", main_cpp
    )
    assert automation is not None
    assert re.search(
        rf"{automation.group(1)}->add_action\(delayaction_id\w*\);", main_cpp
    )


def test_interval_registration_order(main_cpp: str) -> None:
    """Intervals register in config order; one waiting for a later id keeps its place."""
    registered = re.findall(r"App\.register_component_\((\w+), \d+\);", main_cpp)
    assert registered[-6:] == [
        "interval_intervaltrigger_id",
        "interval_intervaltrigger_id_2",
        "interval_intervaltrigger_id_3",
        "named_interval",
        "interval_intervaltrigger_id_4",
        "some_sensor",
    ]
    assert main_cpp.index(
        "App.register_component_(interval_intervaltrigger_id_4"
    ) < main_cpp.index("new(later_script)")
