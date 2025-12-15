import esphome.codegen as cg
from esphome.components import web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_TARGET_TEMPERATURE,
    CONF_VISUAL,
)

CODEOWNERS = ["@dhoeben"]

IS_PLATFORM_COMPONENT = True
water_heater_ns = cg.esphome_ns.namespace("water_heater")
WaterHeater = water_heater_ns.class_("WaterHeater", cg.EntityBase)
WaterHeaterCall = water_heater_ns.class_("WaterHeaterCall")

WaterHeaterMode = water_heater_ns.enum("WaterHeaterMode")
WATER_HEATER_MODES = {
    "OFF": WaterHeaterMode.WATER_HEATER_MODE_OFF,
    "ECO": WaterHeaterMode.WATER_HEATER_MODE_ECO,
    "ELECTRIC": WaterHeaterMode.WATER_HEATER_MODE_ELECTRIC,
    "PERFORMANCE": WaterHeaterMode.WATER_HEATER_MODE_PERFORMANCE,
    "HIGH_DEMAND": WaterHeaterMode.WATER_HEATER_MODE_HIGH_DEMAND,
    "HEAT_PUMP": WaterHeaterMode.WATER_HEATER_MODE_HEAT_PUMP,
    "GAS": WaterHeaterMode.WATER_HEATER_MODE_GAS,
}

validate_water_heater_mode = cv.enum(WATER_HEATER_MODES, upper=True)

WATER_HEATER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(WaterHeater),
        cv.Optional(CONF_MIN_TEMPERATURE): cv.temperature,
        cv.Optional(CONF_MAX_TEMPERATURE): cv.temperature,
        cv.Optional(CONF_TARGET_TEMPERATURE): cv.temperature,
        cv.Optional(CONF_VISUAL, default={}): cv.Schema(
            {
                cv.Optional(CONF_MIN_TEMPERATURE): cv.temperature,
                cv.Optional(CONF_MAX_TEMPERATURE): cv.temperature,
            }
        ),
    }
)


async def register_water_heater(var, config):
    await cg.register_component(var, config)
    cg.add(cg.App.register_water_heater(var))

    if CONF_MIN_TEMPERATURE in config:
        cg.add(var.set_visual_min_temperature_override(config[CONF_MIN_TEMPERATURE]))
    if CONF_MAX_TEMPERATURE in config:
        cg.add(var.set_visual_max_temperature_override(config[CONF_MAX_TEMPERATURE]))
    if CONF_VISUAL in config:
        visual = config[CONF_VISUAL]
        if CONF_MIN_TEMPERATURE in visual:
            cg.add(
                var.set_visual_min_temperature_override(visual[CONF_MIN_TEMPERATURE])
            )
        if CONF_MAX_TEMPERATURE in visual:
            cg.add(
                var.set_visual_max_temperature_override(visual[CONF_MAX_TEMPERATURE])
            )
