import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_LIGHT, DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv

from .. import gree_ns
from ..climate import CONF_MODEL, GreeClimate

CODEOWNERS = ["@nagyrobi"]

GreeModeBitSwitch = gree_ns.class_("GreeModeBitSwitch", switch.Switch, cg.Component)

CONF_TURBO = "turbo"
CONF_HEALTH = "health"
CONF_XFAN = "xfan"
CONF_GREE_ID = "gree_id"

# Switch configurations: (config_key, display_name, bit_mask, icon)
SWITCH_CONFIGS = (
    (CONF_TURBO, "Gree Turbo Switch", 0x10, "mdi:car-turbocharger"),
    (CONF_LIGHT, "Gree Light Switch", 0x20, "mdi:led-outline"),
    (CONF_HEALTH, "Gree Health Switch", 0x40, "mdi:pine-tree"),
    (CONF_XFAN, "Gree X-FAN Switch", 0x80, "mdi:wall-sconce-flat"),
)

SUPPORTED_MODELS = {
    "yan",
    "yaa",
    "yac",
    "yac1fb9",
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
        **{
            cv.Optional(key): switch.switch_schema(
                GreeModeBitSwitch,
                icon=icon,
                default_restore_mode="RESTORE_DEFAULT_OFF",
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key, _, _, icon in SWITCH_CONFIGS
        },
    }
)


def _validate_model(config):
    full_config = fv.full_config.get()
    climate_path = full_config.get_path_for_id(config[CONF_GREE_ID])[:-1]
    climate_conf = full_config.get_config_for_path(climate_path)
    if climate_conf[CONF_MODEL] not in SUPPORTED_MODELS:
        raise cv.Invalid(
            "Gree switches are only supported for the "
            + ", ".join(SUPPORTED_MODELS)
            + " models"
        )


FINAL_VALIDATE_SCHEMA = _validate_model


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GREE_ID])

    for conf_key, name, bit_mask, _ in SWITCH_CONFIGS:
        if switch_conf := config.get(conf_key):
            sw = cg.new_Pvariable(switch_conf[cv.CONF_ID], name, bit_mask)
            await switch.register_switch(sw, switch_conf)
            await cg.register_component(sw, switch_conf)
            await cg.register_parented(sw, parent)
