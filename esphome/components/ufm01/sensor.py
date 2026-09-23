import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FLOW,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_WATER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_LITRE,
)

from . import CONF_UFM01_ID, UFM01Component

DEPENDENCIES = ["ufm01"]

CONF_ACCUMULATED_FLOW = "accumulated_flow"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_UFM01_ID): cv.use_id(UFM01Component),
        cv.Optional(CONF_ACCUMULATED_FLOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_LITRE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_FLOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
            accuracy_decimals=5,
            device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:waves-arrow-right",
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:thermometer-water",
        ),
    }
)


async def to_code(config):
    ufm01_component = await cg.get_variable(config[CONF_UFM01_ID])

    if CONF_ACCUMULATED_FLOW in config:
        sens = await sensor.new_sensor(config[CONF_ACCUMULATED_FLOW])
        cg.add(ufm01_component.set_accumulated_flow_sensor(sens))

    if CONF_FLOW in config:
        sens = await sensor.new_sensor(config[CONF_FLOW])
        cg.add(ufm01_component.set_flow_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(ufm01_component.set_temperature_sensor(sens))
