import esphome.codegen as cg
from esphome.components import climate
from esphome.components.number import Number
from esphome.components.select import Select
from esphome.components.sensor import Sensor
from esphome.components.text_sensor import TextSensor
import esphome.config_validation as cv
from esphome.const import CONF_ACTION_ID, CONF_ID

from .. import template_ns

CONF_AUTO_ACTION = "auto_action"
CONF_CURRENT_TEMPERATURE_ID = "current_temperature_id"
CONF_TARGET_TEMPERATURE_ID = "target_temperature_id"
CONF_MODE_ID = "mode_id"
CONF_FAN_MODE_ID = "fan_mode_id"
CONF_SWING_MODE_ID = "swing_mode_id"
CONF_PRESET_ID = "preset_id"

TemplateClimate = template_ns.class_("TemplateClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(TemplateClimate),
        cv.Required(CONF_CURRENT_TEMPERATURE_ID): cv.use_id(Sensor),
        cv.Required(CONF_TARGET_TEMPERATURE_ID): cv.use_id(Number),
        cv.Required(CONF_MODE_ID): cv.use_id(Select),
        cv.Optional(CONF_FAN_MODE_ID): cv.use_id(Select),
        cv.Optional(CONF_SWING_MODE_ID): cv.use_id(Select),
        cv.Optional(CONF_PRESET_ID): cv.use_id(Select),
        cv.Optional(CONF_ACTION_ID): cv.use_id(TextSensor),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    cg.add(
        var.set_current_temperature(
            await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_ID])
        )
    )

    cg.add(
        var.set_target_temperature(
            await cg.get_variable(config[CONF_TARGET_TEMPERATURE_ID])
        )
    )

    cg.add(var.set_mode(await cg.get_variable(config[CONF_MODE_ID])))

    if v := config.get(CONF_FAN_MODE_ID):
        cg.add(var.set_fan_mode(await cg.get_variable(v)))

    if v := config.get(CONF_SWING_MODE_ID):
        cg.add(var.set_swing_mode(await cg.get_variable(v)))

    if v := config.get(CONF_PRESET_ID):
        cg.add(var.set_preset(await cg.get_variable(v)))

    if v := config.get(CONF_ACTION_ID):
        cg.add(var.set_action(await cg.get_variable(v)))
