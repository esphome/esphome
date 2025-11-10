import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

water_heater_ns = cg.esphome_ns.namespace("water_heater")
WaterHeater = water_heater_ns.class_("WaterHeater", cg.EntityBase, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WaterHeater),
    cv.Optional("current_temperature"): sensor.sensor_schema(),
    cv.Optional("target_temperature"): sensor.sensor_schema(),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var)
    await cg.register_entity(var)

    if "current_temperature" in config:
        sens = await sensor.new_sensor(config["current_temperature"])
        cg.add(var.set_current_temperature_sensor(sens))

    if "target_temperature" in config:
        sens = await sensor.new_sensor(config["target_temperature"])
        cg.add(var.set_target_temperature_sensor(sens))
