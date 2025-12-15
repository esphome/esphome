import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_MIN_TEMPERATURE, CONF_MAX_TEMPERATURE
from esphome.components import sensor

CODEOWNERS = ["@dhoeben"]

water_heater_ns = cg.esphome_ns.namespace('water_heater')
WaterHeater = water_heater_ns.class_('WaterHeater', cg.EntityBase, cg.Component)
TemplateWaterHeater = water_heater_ns.class_('TemplateWaterHeater', WaterHeater)

CONF_ON_MODE_SET = "on_mode_set"
CONF_ON_TEMPERATURE_SET = "on_temperature_set"


BASE_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WaterHeater),
    cv.Optional(CONF_MIN_TEMPERATURE, default=40.0): cv.temperature,
    cv.Optional(CONF_MAX_TEMPERATURE, default=60.0): cv.temperature,
}).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = BASE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TemplateWaterHeater),
    cv.Optional(CONF_ON_MODE_SET): cv.automation_schema,
    cv.Optional(CONF_ON_TEMPERATURE_SET): cv.automation_schema,
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_min_temperature(config[CONF_MIN_TEMPERATURE]))
    cg.add(var.set_max_temperature(config[CONF_MAX_TEMPERATURE]))

    if CONF_ON_MODE_SET in config:
        await cg.automation.build_automation(var.get_mode_trigger(), [(cg.int_, "x")], config[CONF_ON_MODE_SET])
    
    if CONF_ON_TEMPERATURE_SET in config:
        await cg.automation.build_automation(var.get_temperature_trigger(), [(cg.float_, "x")], config[CONF_ON_TEMPERATURE_SET])