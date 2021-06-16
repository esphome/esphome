import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ATTRIBUTE,
    CONF_ENTITY_ID,
    CONF_ID,
    ICON_EMPTY,
    STATE_CLASS_NONE,
    UNIT_EMPTY,
    DEVICE_CLASS_EMPTY,
)
from .. import homeassistant_ns

DEPENDENCIES = ["api"]

HomeassistantSensor = homeassistant_ns.class_(
    "HomeassistantSensor", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = sensor.sensor_schema(
    UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
).extend(
    {
        cv.GenerateID(): cv.declare_id(HomeassistantSensor),
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_ATTRIBUTE): cv.string,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(var.set_attribute(config[CONF_ATTRIBUTE]))
