"""Tests for GREE feature switch validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components import switch
from esphome.components.gree.switch import (
    CONF_GREE_ID,
    CONF_HEALTH,
    CONF_TURBO,
    CONF_XFAN,
    _feature_switch_schema,
    _validate_model,
    _validate_unique,
)
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_LIGHT,
    CONF_MODEL,
    CONF_NAME,
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
    CONF_STATE,
)
from esphome.core import ID
from esphome.types import ConfigType

CONF_CLIMATE = "climate"
RESTORE_MODE_VALIDATOR = cv.enum(switch.RESTORE_MODES, upper=True, space="_")


def _switch_config(model: str, features: ConfigType) -> ConfigType:
    climate_id = ID("gree_climate", is_declaration=True)
    full_config = fv.full_config.get()
    full_config[CONF_CLIMATE] = [{CONF_ID: climate_id, CONF_MODEL: model}]
    full_config.declare_ids.append((climate_id, [CONF_CLIMATE, 0, CONF_ID]))
    return {CONF_GREE_ID: ID("gree_climate", is_declaration=False), **features}


def _validate_switch_config(
    model: str, features: set[str], restore_modes: dict[str, str] | None = None
) -> ConfigType:
    config = _switch_config(
        model,
        {
            feature: (
                {CONF_RESTORE_MODE: RESTORE_MODE_VALIDATOR(restore_modes[feature])}
                if restore_modes and feature in restore_modes
                else {}
            )
            for feature in features
        },
    )
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


@pytest.mark.parametrize(
    ("raw", "expected"),
    [
        pytest.param({}, None, id="omitted"),
        pytest.param(
            {CONF_RESTORE_MODE: "RESTORE_DEFAULT_ON"},
            "RESTORE_DEFAULT_ON",
            id="explicit-schema-default",
        ),
        pytest.param({CONF_RESTORE_MODE: "DISABLED"}, "DISABLED", id="explicit"),
    ],
)
def test_schema_keeps_only_explicit_restore_modes(
    raw: dict[str, str], expected: str | None
) -> None:
    validator = _feature_switch_schema("mdi:led-outline", "RESTORE_DEFAULT_ON")
    config = validator({CONF_NAME: "Light", **raw})
    if expected is None:
        assert CONF_RESTORE_MODE not in config
    else:
        assert str(config[CONF_RESTORE_MODE]) == expected


def test_schema_initial_state_keeps_the_feature_default() -> None:
    validator = _feature_switch_schema("mdi:led-outline", "RESTORE_DEFAULT_ON")
    config = validator({CONF_NAME: "Light", CONF_INITIAL_STATE: False})
    assert CONF_RESTORE_MODE not in config
    assert config[CONF_RESTORE_STATE][CONF_STATE] == switch.RESTORE_STATE_KEEP


@pytest.mark.parametrize("model", ["yb1fa", "yx1ff"])
def test_rx_capable_switches_accept_omitted_or_disabled(model: str) -> None:
    _validate_switch_config(model, {CONF_LIGHT})
    _validate_switch_config(model, {CONF_LIGHT}, {CONF_LIGHT: "DISABLED"})


@pytest.mark.parametrize(
    ("model", "feature", "restore_mode"),
    [
        ("yx1ff", CONF_LIGHT, "RESTORE_DEFAULT_ON"),
        ("yx1ff", CONF_LIGHT, "ALWAYS_ON"),
        ("yx1ff", CONF_LIGHT, "RESTORE_DEFAULT_OFF"),
        ("yb1fa", CONF_TURBO, "RESTORE_DEFAULT_OFF"),
        ("yb1fa", CONF_TURBO, "ALWAYS_OFF"),
        ("yb1fa", CONF_LIGHT, "ALWAYS_ON"),
    ],
)
def test_rx_capable_switches_reject_explicit_restore_modes(
    model: str, feature: str, restore_mode: str
) -> None:
    with pytest.raises(cv.Invalid, match="restore_mode must be DISABLED or omitted"):
        _validate_switch_config(model, {feature}, {feature: restore_mode})


def test_rx_capable_switches_reject_restore_state() -> None:
    with pytest.raises(cv.Invalid, match="restore_mode must be DISABLED or omitted"):
        _validate_model(_switch_config("yx1ff", {CONF_LIGHT: {CONF_RESTORE_STATE: {}}}))


def test_legacy_switches_keep_custom_restore_modes() -> None:
    config = _validate_switch_config("yan", {CONF_LIGHT}, {CONF_LIGHT: "ALWAYS_OFF"})
    assert str(config[CONF_LIGHT][CONF_RESTORE_MODE]) == "ALWAYS_OFF"


def test_omitted_restore_mode_follows_the_model(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("yan_switch.yaml"))
    assert (
        "yan_light->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON);" in main_cpp
    )
    assert (
        "yan_turbo->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF);" in main_cpp
    )
    assert "yan_health->set_restore_mode(switch_::SWITCH_ALWAYS_ON);" in main_cpp
    assert (
        "gree_yan->register_feature_switch(gree::GREE_FEATURE_LIGHT, yan_light);"
        in (main_cpp)
    )


def test_second_switch_entry_for_one_climate_rejected() -> None:
    """The climate keeps one switch per feature, so a second entry for it would take one over."""
    entry = {"platform": "gree", CONF_GREE_ID: "gree_climate"}
    fv.full_config.get()["switch"] = [entry, dict(entry)]

    with pytest.raises(cv.Invalid, match="Only one gree switch entry"):
        _validate_unique(entry)


def test_each_climate_may_have_its_own_switch_entry() -> None:
    """Switch entries of different climates do not compete."""
    entries = [
        {"platform": "gree", CONF_GREE_ID: "gree_a"},
        {"platform": "gree", CONF_GREE_ID: "gree_b"},
    ]
    fv.full_config.get()["switch"] = entries

    for entry in entries:
        _validate_unique(entry)
