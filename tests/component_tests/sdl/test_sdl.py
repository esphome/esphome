"""Tests for the sdl display schema, in particular the headless option."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.sdl.display import (
    CONF_SDL_ID,
    CONFIG_SCHEMA,
    headless_final_validate,
)
from esphome.config import Config
from esphome.const import PlatformFramework
from esphome.core import ID
from esphome.final_validate import full_config
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


def test_headless_rejects_snapshot_key() -> None:
    """A headless display has no keyboard, so the action is the only way in."""
    with pytest.raises(cv.Invalid, match="snapshot.take"):
        CONFIG_SCHEMA(_config(headless=True, snapshot_key="SDLK_F12"))


def test_snapshot_key_accepted_when_windowed() -> None:
    """The key is only valid alongside a window."""
    config = CONFIG_SCHEMA(_config(snapshot_key="SDLK_F12"))
    assert str(config["snapshot_key"]) == "SDLK_F12"


def _declare_sdl_display(headless: bool) -> ID:
    """Register a full_config with a single sdl display declaration and return a reference to it.

    Mirrors what the real config pipeline leaves behind: a "display" domain entry plus a
    declare_ids record id_declaration_match_schema uses to find it again.
    """
    declared_id = ID("my_sdl", is_declaration=True)
    fc = Config()
    fc["display"] = [
        {
            "platform": "sdl",
            "id": declared_id,
            "headless": headless,
            "dimensions": {"width": 320, "height": 240},
        }
    ]
    fc.declare_ids.append((declared_id, ["display", 0, "id"]))
    full_config.set(fc)
    return ID("my_sdl")


@pytest.mark.parametrize("platform", ["binary_sensor", "touchscreen"])
def test_headless_final_validate_rejects_headless_display(platform: str) -> None:
    """binary_sensor and touchscreen both need a window, so a headless display is rejected."""
    sdl_ref = _declare_sdl_display(headless=True)
    schema = headless_final_validate(platform)
    with pytest.raises(cv.Invalid, match="needs a window"):
        schema({CONF_SDL_ID: sdl_ref})


@pytest.mark.parametrize("platform", ["binary_sensor", "touchscreen"])
def test_headless_final_validate_accepts_windowed_display(platform: str) -> None:
    """The same platforms are accepted once the display has a window."""
    sdl_ref = _declare_sdl_display(headless=False)
    schema = headless_final_validate(platform)
    schema({CONF_SDL_ID: sdl_ref})  # Should not raise.
