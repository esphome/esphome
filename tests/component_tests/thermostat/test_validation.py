"""Tests for the thermostat config validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE


def test_dry_only_config_is_valid(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A dehumidifier has dry mode but no cool_action; dry mode uses the cooling timers."""
    main_cpp = generate_main(component_config_path("dry_only.yaml"))

    assert "dehumidifier->set_cooling_minimum_off_time_in_sec(0);" in main_cpp
    assert "dehumidifier->set_cooling_minimum_run_time_in_sec(0);" in main_cpp
    assert "dehumidifier->set_supports_dry(true);" in main_cpp
    assert "dehumidifier->set_supports_cool(" not in main_cpp


def test_cooling_timer_needs_cool_or_dry_action(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A cooling timer with neither cool_action nor dry_action is still rejected."""
    CORE.config_path = component_config_path("cooling_timer_without_action.yaml")
    assert read_config({}) is None
    captured = capsys.readouterr()
    assert (
        "cool_action or dry_action must be defined to use min_cooling_off_time"
        in captured.out
    )
