import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
import esphome.final_validate as fv

from ..climate import CONF_MODEL, GreeClimate, Model
from .. import gree_ns

CODEOWNERS = ["@orestismers"]
DEPENDENCIES = ["gree"]

GreeTurboSwitch = gree_ns.class_("GreeTurboSwitch", switch.Switch)
GreeLightSwitch = gree_ns.class_("GreeLightSwitch", switch.Switch)
GreeHealthSwitch = gree_ns.class_("GreeHealthSwitch", switch.Switch)
GreeXfanSwitch = gree_ns.class_("GreeXfanSwitch", switch.Switch)

CONF_TURBO = "turbo"
CONF_LIGHT = "light"
CONF_HEALTH_MODE = "health"
CONF_XFAN = "xfan"
CONF_GREE_ID = "gree_id"

SUPPORTED_MODELS = {
    Model.GREE_YAN,
    Model.GREE_YAA,
    Model.GREE_YAC,
    Model.GREE_YAC1FB9,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate),
        cv.Optional(CONF_TURBO): switch.switch_schema(
            GreeTurboSwitch,
            icon="mdi:car-turbocharger",
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
        cv.Optional(CONF_LIGHT): switch.switch_schema(
            GreeLightSwitch,
            icon="mdi:led-outline",
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
        cv.Optional(CONF_HEALTH_MODE): switch.switch_schema(
            GreeHealthSwitch,
            icon="mdi:tree-outline",
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
        cv.Optional(CONF_XFAN): switch.switch_schema(
            GreeXfanSwitch,
            icon="mdi:wall-sconce-flat",
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
    }
)


def _validate_model(config):
    full_config = fv.full_config.get()
    climate_path = full_config.get_path_for_id(config[CONF_GREE_ID])[:-1]
    climate_conf = full_config.get_config_for_path(climate_path)
    if climate_conf is None:
        raise cv.Invalid("Gree climate reference is invalid")
    model = climate_conf.get(CONF_MODEL)
    if model is None:
        raise cv.Invalid("Gree climate model is not configured")

    if model not in SUPPORTED_MODELS:
        raise cv.Invalid("Gree switches are only supported for the yan, yaa, yac and yac1fb9 models")

    return config


FINAL_VALIDATE_SCHEMA = _validate_model


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GREE_ID])

    if turbo_conf := config.get(CONF_TURBO):
        turbo_switch = await switch.new_switch(turbo_conf)
        await cg.register_parented(turbo_switch, parent)
        cg.add(parent.set_turbo_switch(turbo_switch))
    if light_conf := config.get(CONF_LIGHT):
        light_switch = await switch.new_switch(light_conf)
        await cg.register_parented(light_switch, parent)
        cg.add(parent.set_light_switch(light_switch))
    if health_conf := config.get(CONF_HEALTH_MODE):
        health_switch = await switch.new_switch(health_conf)
        await cg.register_parented(health_switch, parent)
        cg.add(parent.set_health_switch(health_switch))
    if xfan_conf := config.get(CONF_XFAN):
        xfan_switch = await switch.new_switch(xfan_conf)
        await cg.register_parented(xfan_switch, parent)
        cg.add(parent.set_xfan_switch(xfan_switch))

