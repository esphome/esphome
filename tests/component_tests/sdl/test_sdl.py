"""Tests for the sdl display schema, in particular the headless option."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.sdl.display import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture(autouse=True)
def _host_platform(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.HOST_NATIVE)


def _config(**extra: object) -> ConfigType:
    config: ConfigType = {
        "dimensions": {"width": 320, "height": 240},
        # sdl2-config is not necessarily installed in the test environment
        "sdl_options": "-lSDL2",
    }
    config.update(extra)
    return config


def test_defaults_to_windowed() -> None:
    """A display without the option is not headless."""
    assert CONFIG_SCHEMA(_config())["headless"] is False


def test_headless_accepted() -> None:
    """A headless display needs nothing beyond the dimensions."""
    assert CONFIG_SCHEMA(_config(headless=True))["headless"] is True


def test_headless_rejects_window_options() -> None:
    """Window options are meaningless without a window."""
    with pytest.raises(cv.Invalid, match="has no effect"):
        CONFIG_SCHEMA(
            _config(headless=True, window_options={"position": {"x": 0, "y": 0}})
        )


def test_headless_rejects_screenshot_key() -> None:
    """A headless display has no keyboard, so the action is the only way in."""
    with pytest.raises(cv.Invalid, match="sdl.screenshot"):
        CONFIG_SCHEMA(_config(headless=True, screenshot_key="SDLK_F12"))


def test_screenshot_key_accepted_when_windowed() -> None:
    """The key is only valid alongside a window."""
    config = CONFIG_SCHEMA(_config(screenshot_key="SDLK_F12"))
    assert str(config["screenshot_key"]) == "SDLK_F12"
