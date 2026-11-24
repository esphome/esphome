"""Tests for the code generated for interval blocks."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from tests.component_tests.helpers import (
    callback_function,
    constructed,
    delayed_automation,
)

CONFIG = "interval_codegen.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path(CONFIG))


@pytest.mark.parametrize(
    ("var", "startup_delay", "update_interval"),
    [
        ("interval_intervalcallback_id", 0, 1000),
        ("interval_intervalcallback_id_2", 5000, 2000),
        ("interval_intervalcallback_id_3", 0, 3000),
        ("named_interval", 3000, 4000),
        ("interval_intervalcallback_id_4", 0, 5000),
    ],
)
def test_interval_constructed(
    main_cpp: str, var: str, startup_delay: int, update_interval: int
) -> None:
    """Each interval is built with its startup delay, if any, and gets its update interval."""
    function = callback_function(main_cpp, var)
    expected = (
        f"interval::DelayedIntervalCallback<{function}, {startup_delay}>()"
        if startup_delay
        else f"interval::IntervalCallback<{function}>()"
    )
    assert constructed(main_cpp, var) == expected
    assert f"{var}->set_update_interval({update_interval});" in main_cpp


def test_interval_delay_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation of separate actions, which the function triggers."""
    delayed_automation(
        main_cpp, callback_function(main_cpp, "interval_intervalcallback_id_3")
    )


def test_interval_registration_order(main_cpp: str) -> None:
    """Intervals register in config order; one waiting for a later id keeps its place."""
    registered = re.findall(r"App\.register_component_\((\w+), \d+\);", main_cpp)
    assert registered[-6:] == [
        "interval_intervalcallback_id",
        "interval_intervalcallback_id_2",
        "interval_intervalcallback_id_3",
        "named_interval",
        "interval_intervalcallback_id_4",
        "some_sensor",
    ]
    assert main_cpp.index(
        "App.register_component_(interval_intervalcallback_id_4"
    ) < main_cpp.index("new(later_script)")
