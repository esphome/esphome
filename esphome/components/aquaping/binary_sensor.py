import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_EMPTY,
)

from . import CONF_AQUAPING_ID, AQUAPINGComponent

optional_sensors = {
    "alarm": {
        "target": "set_alarm_binary_sensor",
        "config": binary_sensor.binary_sensor_schema(
            # unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_EMPTY,
        ),
    },
    "noise_alert": {
        "target": "set_noise_alert_binary_sensor",
        "config": binary_sensor.binary_sensor_schema(
            # unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_EMPTY,
        ),
    },
    "led": {
        "target": "set_led_binary_sensor",
        "config": binary_sensor.binary_sensor_schema(
            # unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_EMPTY,
        ),
    },
    "sleep": {
        "target": "set_sleep_binary_sensor",
        "config": binary_sensor.binary_sensor_schema(
            # unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_EMPTY,
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
            # print("HELLO2", key)
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(parent_component, data_dict["target"])(sens))
