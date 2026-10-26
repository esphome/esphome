import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_LIGHT,
    CONF_RESTORE_MODE,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import gree_ns
from ..climate import CONF_MODEL, GreeClimate

CODEOWNERS = ["@nagyrobi"]

GreeFeatureSwitch = gree_ns.class_("GreeFeatureSwitch", switch.Switch, cg.Component)
GreeFeature = gree_ns.enum("GreeFeature")

CONF_TURBO = "turbo"
CONF_HEALTH = "health"
CONF_XFAN = "xfan"
CONF_GREE_ID = "gree_id"

# Switch configurations: (config_key, display_name, feature, icon, default_restore_mode)
SWITCH_CONFIGS = (
    (
        CONF_TURBO,
        "Gree Turbo Switch",
        GreeFeature.GREE_FEATURE_TURBO,
        "mdi:car-turbocharger",
        "RESTORE_DEFAULT_OFF",
    ),
    # The display light was on before the switches existed, so it stays on by default
    (
        CONF_LIGHT,
        "Gree Light Switch",
        GreeFeature.GREE_FEATURE_LIGHT,
        "mdi:led-outline",
        "RESTORE_DEFAULT_ON",
    ),
    (
        CONF_HEALTH,
        "Gree Health Switch",
        GreeFeature.GREE_FEATURE_HEALTH,
        "mdi:pine-tree",
        "RESTORE_DEFAULT_OFF",
    ),
    (
        CONF_XFAN,
        "Gree X-FAN Switch",
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
        **{
            cv.Optional(key): switch.switch_schema(
                GreeFeatureSwitch,
                icon=icon,
                default_restore_mode=default_restore_mode,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key, _, _, icon, default_restore_mode in SWITCH_CONFIGS
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
    for feature, *_, default_restore_mode in SWITCH_CONFIGS:
        if feature not in config:
            continue
        # switch_schema fills in the default, so only a changed value can be told apart
        restore_mode = config[feature].get(CONF_RESTORE_MODE, default_restore_mode)
        if str(restore_mode) not in (default_restore_mode, "DISABLED"):
            raise cv.Invalid(
                f"Gree model {model} receives {feature} state from IR; "
                "restore_mode must be DISABLED or omitted",
                path=[feature, CONF_RESTORE_MODE],
            )


FINAL_VALIDATE_SCHEMA = _validate_model


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_GREE_ID])
    receives_state = (
        fv.get_declaring_config(config[CONF_GREE_ID], CORE.config)[CONF_MODEL]
        in RX_FEATURE_MODELS
    )
    cg.add_define("USE_GREE_FEATURE_SWITCH")

    for conf_key, name, feature, *_ in SWITCH_CONFIGS:
        if switch_conf := config.get(conf_key):
            if receives_state:
                switch_conf = {
                    **switch_conf,
                    CONF_RESTORE_MODE: switch.RESTORE_MODES["DISABLED"],
                }
            sw = cg.new_Pvariable(switch_conf[cv.CONF_ID], name, feature)
            await switch.register_switch(sw, switch_conf)
            await cg.register_component(sw, switch_conf)
            await cg.register_parented(sw, parent)
            cg.add(parent.register_feature_switch(feature, sw))
