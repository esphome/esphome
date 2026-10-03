import pytest

from esphome.components.ssd1306_i2c.display import (
    CONF_PARTIAL_UPDATES,
    PARTIAL_UPDATE_MODELS,
    _validate_partial_updates,
)
import esphome.config_validation as cv
from esphome.const import CONF_MODEL


@pytest.mark.parametrize("model", sorted(PARTIAL_UPDATE_MODELS))
def test_partial_updates_accepts_supported_models(model):
    config = {
        CONF_MODEL: model,
        CONF_PARTIAL_UPDATES: True,
    }

    assert _validate_partial_updates(config) is config


@pytest.mark.parametrize(
    "model",
    [
        "SH1106_128X64",
        "SH1107_128X64",
        "SSD1305_128X64",
    ],
)
def test_partial_updates_rejects_unsupported_models(model):
    config = {
        CONF_MODEL: model,
        CONF_PARTIAL_UPDATES: True,
    }

    with pytest.raises(
        cv.Invalid,
        match="partial_updates is currently supported only for SSD1306 models",
    ):
        _validate_partial_updates(config)


def test_partial_updates_disabled_allows_unsupported_model():
    config = {
        CONF_MODEL: "SH1106_128X64",
        CONF_PARTIAL_UPDATES: False,
    }

    assert _validate_partial_updates(config) is config
