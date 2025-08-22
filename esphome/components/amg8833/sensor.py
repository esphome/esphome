import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from . import AMG8833, CONF_AMG8833_ID

DEPENDENCIES = ["amg8833"]

CONF_AMBIENT = "ambient"
CONF_MAXIMUM = "maximum"
CONF_MINIMUM = "minimum"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833),
    cv.Optional(CONF_AMBIENT): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MAXIMUM): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon="mdi:thermometer-high",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MINIMUM): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon="mdi:thermometer-low",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}


async def to_code(config):
    amg8833_component = await cg.get_variable(config[CONF_AMG8833_ID])
    if conf := config.get(CONF_AMBIENT):
        sens = await sensor.new_sensor(conf)
        cg.add(amg8833_component.set_ambient_sensor(sens))
    if conf := config.get(CONF_MAXIMUM):
        sens = await sensor.new_sensor(conf)
        cg.add(amg8833_component.set_maximum_sensor(sens))
    if conf := config.get(CONF_MINIMUM):
        sens = await sensor.new_sensor(conf)
        cg.add(amg8833_component.set_minimum_sensor(sens))
