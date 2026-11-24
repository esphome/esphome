"""Tests for the interval component."""

from collections.abc import Callable
from pathlib import Path


def test_interval_startup_delay_class_per_entry(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only an interval with a non-zero startup delay uses DelayedIntervalTrigger."""
    main_cpp = generate_main(component_config_path("interval_test.yaml"))

    assert "new(no_delay) interval::IntervalTrigger();" in main_cpp
    assert "new(zero_delay) interval::IntervalTrigger();" in main_cpp
    assert "new(delayed) interval::DelayedIntervalTrigger(5000);" in main_cpp
    assert "static interval::DelayedIntervalTrigger *const delayed = " in main_cpp
    assert "set_startup_delay" not in main_cpp
