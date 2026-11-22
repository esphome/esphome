"""Tests for the interval component."""

from collections.abc import Callable
from pathlib import Path


def test_interval_startup_delay_class_per_entry(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only an interval with a non-zero startup delay uses DelayedIntervalCallback."""
    main_cpp = generate_main(component_config_path("interval_test.yaml"))

    assert (
        "new(no_delay) interval::IntervalCallback<esphome__no_delay__fn>();" in main_cpp
    )
    assert (
        "new(zero_delay) interval::IntervalCallback<esphome__zero_delay__fn>();"
        in main_cpp
    )
    delayed = "interval::DelayedIntervalCallback<esphome__delayed__fn, 5000>"
    assert f"new(delayed) {delayed}();" in main_cpp
    assert f"static {delayed} *const delayed = " in main_cpp
    assert "set_startup_delay" not in main_cpp
