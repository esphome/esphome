import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_PLATFORM

water_heater_ns = cg.esphome_ns.namespace("water_heater")
WaterHeater = water_heater_ns.class_("WaterHeater", cg.EntityBase, cg.Component)

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({})

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.Schema({
            cv.Required(CONF_PLATFORM): cv.one_of("thermostat", "template", "opentherm", lower=True),
        })
    )
)

async def to_code(config):
    for conf in config:
        platform = conf[CONF_PLATFORM]
        await cg.get_platform("water_heater", platform)(conf)
