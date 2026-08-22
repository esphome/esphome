#  YAML config keys
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.cpp_generator import MockObj

from . import CONF_LSM6DS_ID, LSM6DSComponent

CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_TEMPERATURE,
).extend(
    {
        cv.Optional(CONF_TYPE): CONF_TEMPERATURE,
        cv.GenerateID(CONF_LSM6DS_ID): cv.use_id(LSM6DSComponent),
    }
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_LSM6DS_ID])
    data = MockObj("data")
    value_lambda = await cg.process_lambda(
        var.publish_state(data),
        [(cg.float_, str(data))],
    )
    cg.add(parent.add_temperature_listener(value_lambda))
