"""Tests for the st7305 configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.st7305.display import (
    CONF_COLUMN_END,
    CONF_COLUMN_START,
    CONF_PORTRAIT,
    CONF_ROW_END,
    MODELS,
    _resolve,
)
from esphome.const import CONF_CS_PIN, CONF_DC_PIN, CONF_HEIGHT, CONF_WIDTH


def _config(**overrides):
    config = {
        CONF_WIDTH: 400,
        CONF_HEIGHT: 300,
        CONF_PORTRAIT: False,
        CONF_CS_PIN: 1,
        CONF_DC_PIN: 2,
    }
    config.update(overrides)
    return config


def test_landscape_derives_window() -> None:
    config = _config()
    _resolve(config, MODELS["CUSTOM"])
    assert config[CONF_COLUMN_START] == 0
    assert config[CONF_COLUMN_END] == 24
    assert config[CONF_ROW_END] == 199


@pytest.mark.parametrize("height", [160, 200, 296, 308])
def test_landscape_rejects_inner_not_multiple_of_12(height: int) -> None:
    with pytest.raises(cv.Invalid, match="height divisible by 12"):
        _resolve(_config(**{CONF_HEIGHT: height}), MODELS["CUSTOM"])


def test_portrait_derives_window() -> None:
    config = _config(
        **{
            CONF_WIDTH: 168,
            CONF_HEIGHT: 384,
            CONF_PORTRAIT: True,
            CONF_COLUMN_START: 0x17,
        }
    )
    _resolve(config, MODELS["CUSTOM"])
    assert config[CONF_COLUMN_END] == 0x24
    assert config[CONF_ROW_END] == 191


@pytest.mark.parametrize("width", [160, 200, 208])
def test_portrait_rejects_inner_not_multiple_of_12(width: int) -> None:
    with pytest.raises(cv.Invalid, match="width divisible by 12"):
        _resolve(
            _config(
                **{
                    CONF_WIDTH: width,
                    CONF_HEIGHT: 384,
                    CONF_PORTRAIT: True,
                }
            ),
            MODELS["CUSTOM"],
        )


def test_window_ends_must_fit_in_a_byte() -> None:
    with pytest.raises(cv.Invalid, match="between 0 and 255"):
        _resolve(_config(**{CONF_WIDTH: 1024, CONF_HEIGHT: 300}), MODELS["CUSTOM"])
