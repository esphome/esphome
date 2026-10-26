"""Tests for GREE feature switch validation."""

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components import switch
from esphome.components.gree.switch import (
    CONF_GREE_ID,
    CONF_HEALTH,
    CONF_TURBO,
    CONF_XFAN,
    _validate_model,
)
from esphome.const import CONF_ID, CONF_LIGHT, CONF_MODEL, CONF_RESTORE_MODE
from esphome.core import ID
from esphome.types import ConfigType

CONF_CLIMATE = "climate"
RESTORE_MODE_VALIDATOR = cv.enum(switch.RESTORE_MODES, upper=True, space="_")


def _validate_switch_config(
    model: str, features: set[str], restore_modes: dict[str, str] | None = None
) -> ConfigType:
    climate_id = ID("gree_climate", is_declaration=True)
    full_config = fv.full_config.get()
    full_config[CONF_CLIMATE] = [{CONF_ID: climate_id, CONF_MODEL: model}]
    full_config.declare_ids.append((climate_id, [CONF_CLIMATE, 0, CONF_ID]))

    config = {
        CONF_GREE_ID: ID("gree_climate", is_declaration=False),
        **{
            feature: (
                {CONF_RESTORE_MODE: RESTORE_MODE_VALIDATOR(restore_modes[feature])}
                if restore_modes and feature in restore_modes
                else {}
            )
            for feature in features
        },
    }
    _validate_model(config)
    return config


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
def test_rx_capable_switches_do_not_mutate_restore_config(model: str) -> None:
    config = _validate_switch_config(model, {CONF_LIGHT})
    assert CONF_RESTORE_MODE not in config[CONF_LIGHT]


@pytest.mark.parametrize(
    ("model", "feature", "restore_mode"),
    [
        ("yx1ff", CONF_LIGHT, "RESTORE_DEFAULT_ON"),
        ("yb1fa", CONF_TURBO, "RESTORE_DEFAULT_OFF"),
        ("yb1fa", CONF_LIGHT, "DISABLED"),
    ],
)
def test_rx_capable_switches_accept_schema_defaults_or_disabled(
    model: str, feature: str, restore_mode: str
) -> None:
    config = _validate_switch_config(model, {feature}, {feature: restore_mode})
    assert str(config[feature][CONF_RESTORE_MODE]) == restore_mode


@pytest.mark.parametrize(
    ("model", "feature", "restore_mode"),
    [
        ("yx1ff", CONF_LIGHT, "ALWAYS_ON"),
        ("yx1ff", CONF_LIGHT, "RESTORE_DEFAULT_OFF"),
        ("yb1fa", CONF_TURBO, "ALWAYS_OFF"),
        ("yb1fa", CONF_LIGHT, "ALWAYS_ON"),
    ],
)
def test_rx_capable_switches_reject_custom_restore_modes(
    model: str, feature: str, restore_mode: str
) -> None:
    with pytest.raises(cv.Invalid, match="restore_mode must be DISABLED or omitted"):
        _validate_switch_config(model, {feature}, {feature: restore_mode})


def test_legacy_switches_keep_custom_restore_modes() -> None:
    config = _validate_switch_config("yan", {CONF_LIGHT}, {CONF_LIGHT: "ALWAYS_OFF"})
    assert str(config[CONF_LIGHT][CONF_RESTORE_MODE]) == "ALWAYS_OFF"
