import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_LIGHT,
    CONF_RESTORE_MODE,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)
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

# Switch configurations: (config_key, display_name, feature, icon)
SWITCH_CONFIGS = (
    (
        CONF_TURBO,
        "Gree Turbo Switch",
        GreeFeature.GREE_FEATURE_TURBO,
        "mdi:car-turbocharger",
    ),
    (
        CONF_LIGHT,
        "Gree Light Switch",
        GreeFeature.GREE_FEATURE_LIGHT,
        "mdi:led-outline",
    ),
    (
        CONF_HEALTH,
        "Gree Health Switch",
        GreeFeature.GREE_FEATURE_HEALTH,
        "mdi:pine-tree",
    ),
    (
        CONF_XFAN,
        "Gree X-FAN Switch",
        GreeFeature.GREE_FEATURE_XFAN,
        "mdi:wall-sconce-flat",
    ),
)

MODEL_FEATURES = {
    "yan": frozenset({CONF_TURBO, CONF_LIGHT, CONF_HEALTH, CONF_XFAN}),
    "yaa": frozenset({CONF_TURBO, CONF_LIGHT, CONF_HEALTH, CONF_XFAN}),
    "yac": frozenset({CONF_TURBO, CONF_LIGHT, CONF_HEALTH, CONF_XFAN}),
    "yac1fb9": frozenset({CONF_TURBO, CONF_LIGHT, CONF_HEALTH, CONF_XFAN}),
    "yb1fa": frozenset({CONF_TURBO, CONF_LIGHT, CONF_XFAN}),
    "yx1ff": frozenset({CONF_LIGHT}),
}
RX_FEATURE_MODELS = frozenset({"yb1fa", "yx1ff"})
RX_RESTORE_MODE = cv.enum(switch.RESTORE_MODES, upper=True, space="_")("DISABLED")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
        **{
            cv.Optional(key): switch.switch_schema(
                GreeFeatureSwitch,
                icon=icon,
                default_restore_mode="RESTORE_DEFAULT_OFF",
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key, _, _, icon in SWITCH_CONFIGS
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
    full_config = fv.full_config.get()
    climate_path = full_config.get_path_for_id(config[CONF_GREE_ID])[:-1]
    climate_conf = full_config.get_config_for_path(climate_path)
    model = climate_conf[CONF_MODEL]
    _validate_features_for_model(model, config)
    if model in RX_FEATURE_MODELS:
        for feature, *_ in SWITCH_CONFIGS:
            if feature in config:
                config[feature][CONF_RESTORE_MODE] = RX_RESTORE_MODE


FINAL_VALIDATE_SCHEMA = _validate_model


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_GREE_ID])

    for conf_key, name, feature, _ in SWITCH_CONFIGS:
        if switch_conf := config.get(conf_key):
            sw = cg.new_Pvariable(switch_conf[cv.CONF_ID], name, feature)
            await switch.register_switch(sw, switch_conf)
            await cg.register_component(sw, switch_conf)
            await cg.register_parented(sw, parent)
            cg.add(parent.register_feature_switch(feature, sw))
