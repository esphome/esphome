"""Tests for GREE feature switch validation."""

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components.gree.switch import (
    CONF_GREE_ID,
    CONF_HEALTH,
    CONF_TURBO,
    CONF_XFAN,
    MODEL_FEATURES,
    _validate_model,
)
from esphome.config import Config
from esphome.const import CONF_ID, CONF_LIGHT, CONF_MODEL, CONF_RESTORE_MODE
from esphome.core import ID
from esphome.types import ConfigType

CONF_CLIMATE = "climate"


def _validate_switch_config(model: str, features: set[str]) -> ConfigType:
    climate_id = ID("gree_climate", is_declaration=True)
    full_config = Config()
    full_config[CONF_CLIMATE] = [{CONF_ID: climate_id, CONF_MODEL: model}]
    full_config.declare_ids.append((climate_id, [CONF_CLIMATE, 0, CONF_ID]))

    token = fv.full_config.set(full_config)
    try:
        config = {
            CONF_GREE_ID: ID("gree_climate", is_declaration=False),
            **{feature: {} for feature in features},
        }
        _validate_model(config)
        return config
    finally:
        fv.full_config.reset(token)


def test_feature_capabilities_are_model_specific() -> None:
    assert MODEL_FEATURES["yb1fa"] == {
        CONF_TURBO,
        CONF_LIGHT,
        CONF_XFAN,
    }
    assert MODEL_FEATURES["yx1ff"] == {CONF_LIGHT}


@pytest.mark.parametrize(
    ("model", "feature"),
    [
        pytest.param("yx1ff", CONF_TURBO, id="yx1ff-turbo"),
        pytest.param("yx1ff", CONF_HEALTH, id="yx1ff-health"),
        pytest.param("yb1fa", CONF_HEALTH, id="yb1fa-health"),
        pytest.param("yx1ff", CONF_XFAN, id="yx1ff-xfan"),
    ],
)
def test_unsupported_feature_is_rejected(model: str, feature: str) -> None:
    with pytest.raises(
        cv.Invalid,
        match=rf"Gree model {model} does not support the {feature} switch",
    ):
        _validate_switch_config(model, {feature})


@pytest.mark.parametrize(
    ("model", "features"),
    [
        pytest.param("yx1ff", {CONF_LIGHT}, id="yx1ff-light"),
        pytest.param(
            "yb1fa",
            {CONF_TURBO, CONF_LIGHT, CONF_XFAN},
            id="yb1fa-all-supported",
        ),
        pytest.param(
            "yan",
            {CONF_TURBO, CONF_LIGHT, CONF_HEALTH, CONF_XFAN},
            id="legacy-all-supported",
        ),
    ],
)
def test_supported_features_are_accepted(model: str, features: set[str]) -> None:
    _validate_switch_config(model, features)


@pytest.mark.parametrize("model", ["yb1fa", "yx1ff"])
def test_rx_capable_switches_disable_restore(model: str) -> None:
    config = _validate_switch_config(model, {CONF_LIGHT})
    assert config[CONF_LIGHT][CONF_RESTORE_MODE] == "DISABLED"
