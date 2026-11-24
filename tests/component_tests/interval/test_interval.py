"""Tests for the interval component."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.helpers import callback_function, constructed


def test_interval_startup_delay_class_per_entry(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only an interval with a non-zero startup delay uses DelayedIntervalCallback."""
    main_cpp = generate_main(component_config_path("interval_test.yaml"))

    no_delay = callback_function(main_cpp, "no_delay")
    assert (
        constructed(main_cpp, "no_delay") == f"interval::IntervalCallback<{no_delay}>()"
    )
    zero_delay = callback_function(main_cpp, "zero_delay")
    assert constructed(main_cpp, "zero_delay") == (
        f"interval::IntervalCallback<{zero_delay}>()"
    )
    delayed = (
        "interval::DelayedIntervalCallback"
        f"<{callback_function(main_cpp, 'delayed')}, 5000>"
    )
    assert constructed(main_cpp, "delayed") == f"{delayed}()"
    assert f"static {delayed} *const delayed = " in main_cpp
    assert "set_startup_delay" not in main_cpp
