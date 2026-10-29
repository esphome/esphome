"""Tests for the sun on_sunrise and on_sunset code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.sun import ELEVATION_AUTOMATIONS_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_THEN
from esphome.core import CORE


def _defines() -> dict[str, str]:
    return {define.name: str(define.value) for define in CORE.defines}


def test_triggers_fill_the_sunrise_and_sunset_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each trigger is added to its direction's table with its elevation, and one poll is started."""
    main_cpp = generate_main(component_config_path("triggers.yaml"))

    assert re.findall(
        r"sun_sun_id->add_(\w+)_automation\(automation_id\w*, ([^)]+)\);", main_cpp
    ) == [
        ("sunrise", "-0.83333f"),
        ("sunrise", "5.0f"),
        ("sunset", "-6.0f"),
    ]
    assert main_cpp.count("sun_sun_id->start_elevation_poll();") == 1
    defines = _defines()
    assert "USE_SUN_ELEVATION_POLL" in defines
    assert defines["SUN_SUNRISE_AUTOMATION_COUNT"] == "2"
    assert defines["SUN_SUNSET_AUTOMATION_COUNT"] == "1"


def test_no_triggers_no_poll(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without on_sunrise or on_sunset nothing polls the elevation."""
    main_cpp = generate_main(component_config_path("no_triggers.yaml"))

    assert "start_elevation_poll" not in main_cpp
    assert "_automation(" not in main_cpp
    assert not {
        "USE_SUN_ELEVATION_POLL",
        "SUN_SUNRISE_AUTOMATION_COUNT",
        "SUN_SUNSET_AUTOMATION_COUNT",
    } & set(_defines())


def test_at_most_255_automations_per_direction() -> None:
    """The fill counts are uint8_t, so a 256th automation in one direction is rejected."""
    ELEVATION_AUTOMATIONS_SCHEMA([{CONF_THEN: []}] * 255)
    with pytest.raises(cv.Invalid):
        ELEVATION_AUTOMATIONS_SCHEMA([{CONF_THEN: []}] * 256)
