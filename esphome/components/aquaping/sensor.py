import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_EMPTY,
    UNIT_EMPTY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
)

from . import CONF_AQUAPING_ID, AQUAPINGComponent

optional_sensors = {
    "quiet_count": {
        "target": "set_quiet_count_sensor",
        "config": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    "leak_count": {
        "target": "set_leak_count_sensor",
        "config": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    "noise_count": {
        "target": "set_noise_count_sensor",
        "config": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    "version": {
        "target": "set_version_sensor",
        "config": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_NONE,
        ),
    },
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AQUAPING_ID): cv.use_id(AQUAPINGComponent),
    }
).extend({cv.Optional(k): v["config"] for k, v in optional_sensors.items()})


async def to_code(config):
    parent_component = await cg.get_variable(config[CONF_AQUAPING_ID])

    for key, data_dict in optional_sensors.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(parent_component, data_dict["target"])(sens))
