from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_INITIAL_STATE,
    CONF_LIGHT,
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import gree_ns
from ..climate import CONF_MODEL, GreeClimate

CODEOWNERS = ["@nagyrobi"]

GreeFeature = gree_ns.enum("GreeFeature")

CONF_TURBO = "turbo"
CONF_HEALTH = "health"
CONF_XFAN = "xfan"
CONF_GREE_ID = "gree_id"

# Switch configurations: (config_key, feature, icon, default_restore_mode)
SWITCH_CONFIGS = (
    (
        CONF_TURBO,
        GreeFeature.GREE_FEATURE_TURBO,
        "mdi:car-turbocharger",
        "RESTORE_DEFAULT_OFF",
    ),
    # The display light was on before the switches existed, so it stays on by default
    (
        CONF_LIGHT,
        GreeFeature.GREE_FEATURE_LIGHT,
        "mdi:led-outline",
        "RESTORE_DEFAULT_ON",
    ),
    (
        CONF_HEALTH,
        GreeFeature.GREE_FEATURE_HEALTH,
        "mdi:pine-tree",
        "RESTORE_DEFAULT_OFF",
    ),
    (
        CONF_XFAN,
        GreeFeature.GREE_FEATURE_XFAN,
        "mdi:wall-sconce-flat",
        "RESTORE_DEFAULT_OFF",
    ),
)

ALL_FEATURES = frozenset(key for key, *_ in SWITCH_CONFIGS)
MODEL_FEATURES = {
    "yan": ALL_FEATURES,
    "yaa": ALL_FEATURES,
    "yac": ALL_FEATURES,
    "yac1fb9": ALL_FEATURES,
    "yb1fa": ALL_FEATURES - {CONF_HEALTH},
    "yx1ff": frozenset({CONF_LIGHT}),
}
# These models receive feature state over IR, so the switches follow it instead of restoring
RX_FEATURE_MODELS = frozenset({"yb1fa", "yx1ff"})

_RESTORE_KEYS = (CONF_RESTORE_MODE, CONF_RESTORE_STATE, CONF_INITIAL_STATE)


def _feature_switch_schema(
    icon: str, default_restore_mode: str
) -> Callable[[ConfigType], ConfigType]:
    schema = switch.switch_schema(
        switch.ApplySwitch,
        icon=icon,
        default_restore_mode=default_restore_mode,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
    )

    def validator(value: ConfigType) -> ConfigType:
        explicit = isinstance(value, dict) and any(
            key in value for key in _RESTORE_KEYS
        )
        config = schema(value)
        # Leave an omitted restore mode unset, so final validation can tell it from an explicit one
        # and to_code can fill in the default for the model.
        if not explicit:
            config.pop(CONF_RESTORE_MODE, None)
        return config

    return validator


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
        **{
            cv.Optional(key): _feature_switch_schema(icon, default_restore_mode)
            for key, _, icon, default_restore_mode in SWITCH_CONFIGS
        },
    }
)


def _validate_features_for_model(model: str, config: ConfigType) -> None:
    supported_features = MODEL_FEATURES.get(model)
    if supported_features is None:
        raise cv.Invalid(
            "Gree switches are only supported for the "
            + ", ".join(sorted(MODEL_FEATURES))
            + " models"
        )

    configured_features = {key for key, *_ in SWITCH_CONFIGS if key in config}
    if unsupported := configured_features - supported_features:
        suffix = "switch" if len(unsupported) == 1 else "switches"
        raise cv.Invalid(
            f"Gree model {model} does not support the "
            + ", ".join(sorted(unsupported))
            + f" {suffix}"
        )


def _validate_model(config: ConfigType) -> None:
    model = fv.get_declaring_config(config[CONF_GREE_ID])[CONF_MODEL]
    _validate_features_for_model(model, config)
    if model not in RX_FEATURE_MODELS:
        return
    for feature, *_ in SWITCH_CONFIGS:
        if (feature_conf := config.get(feature)) is None:
            continue
        if (
            CONF_RESTORE_STATE in feature_conf
            or str(feature_conf.get(CONF_RESTORE_MODE, "DISABLED")) != "DISABLED"
        ):
            raise cv.Invalid(
                f"Gree model {model} receives {feature} state from IR; "
                "restore_mode must be DISABLED or omitted",
                path=[feature],
            )


FINAL_VALIDATE_SCHEMA = _validate_model


def _with_default_restore_mode(
    feature_conf: ConfigType, default_restore_mode: str, receives_state: bool
) -> ConfigType:
    """Fill in the model's restore mode when none was configured."""
    if CONF_RESTORE_MODE in feature_conf or CONF_RESTORE_STATE in feature_conf:
        return feature_conf
    mode = "DISABLED" if receives_state else default_restore_mode
    return {**feature_conf, CONF_RESTORE_MODE: switch.RESTORE_MODES[mode]}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_GREE_ID])
    receives_state = (
        fv.get_declaring_config(config[CONF_GREE_ID], CORE.config)[CONF_MODEL]
        in RX_FEATURE_MODELS
    )
    cg.add_define("USE_GREE_FEATURE_SWITCH")

    switches = switch.sub_switches(config, parent=hub)
    for conf_key, feature, _, default_restore_mode in SWITCH_CONFIGS:
        if (feature_conf := config.get(conf_key)) is None:
            continue
        await switches.create(
            _with_default_restore_mode(
                feature_conf, default_restore_mode, receives_state
            ),
            lambda sw, feature=feature: hub.register_feature_switch(feature, sw),
            apply=f"set_feature_state({feature}, state)",
        )
