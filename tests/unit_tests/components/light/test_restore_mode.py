"""Tests for the light restore_mode / initial_state cross-field validation."""

import pytest

from esphome.components.light import LIGHT_SCHEMA
from esphome.components.light.types import LightState
import esphome.config_validation as cv
from esphome.core import ID, Lambda


def _base_config() -> dict:
    return {
        "id": ID("test_light", is_declaration=True, type=LightState),
        "name": "Test",
    }


def test_restore_default_initial_state_requires_initial_state() -> None:
    config = _base_config()
    config["restore_mode"] = "RESTORE_DEFAULT_INITIAL_STATE"
    with pytest.raises(cv.Invalid, match="requires an 'initial_state' block"):
        LIGHT_SCHEMA(config)


def test_restore_default_initial_state_requires_state_key() -> None:
    config = _base_config()
    config["restore_mode"] = "RESTORE_DEFAULT_INITIAL_STATE"
    config["initial_state"] = {"brightness": "50%"}
    with pytest.raises(cv.Invalid, match="requires an 'initial_state' block"):
        LIGHT_SCHEMA(config)


def test_restore_default_initial_state_with_initial_state_is_valid() -> None:
    config = _base_config()
    config["restore_mode"] = "RESTORE_DEFAULT_INITIAL_STATE"
    config["initial_state"] = {"state": "OFF"}
    LIGHT_SCHEMA(config)


@pytest.mark.parametrize(
    "restore_mode",
    ["ALWAYS_OFF", "ALWAYS_ON", "RESTORE_DEFAULT_OFF", "RESTORE_DEFAULT_ON"],
)
def test_other_restore_modes_do_not_require_initial_state(restore_mode: str) -> None:
    config = _base_config()
    config["restore_mode"] = restore_mode
    LIGHT_SCHEMA(config)


def test_initial_state_rejects_lambda() -> None:
    config = _base_config()
    config["initial_state"] = {"state": Lambda("return true;")}
    with pytest.raises(cv.Invalid, match="initial_state values may not be lambdas"):
        LIGHT_SCHEMA(config)
