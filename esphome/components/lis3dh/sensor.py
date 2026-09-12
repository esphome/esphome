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

from . import CONF_LIS3DH_ID, LIS3DHComponent

# The LIS3DH temperature sensor is uncalibrated and only reports relative
# changes (roughly 1 °C per count), so a single decimal of accuracy is plenty.
CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_TEMPERATURE,
).extend(
    {
        cv.Optional(CONF_TYPE): cv.one_of(CONF_TEMPERATURE),
        cv.GenerateID(CONF_LIS3DH_ID): cv.use_id(LIS3DHComponent),
    }
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_LIS3DH_ID])
    data = MockObj("x")
    value_lambda = await cg.process_lambda(
        var.publish_state(data),
        [(cg.float_, str(data))],
    )
    cg.add(parent.add_temperature_listener(value_lambda))
