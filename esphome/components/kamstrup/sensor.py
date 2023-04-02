import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_KELVIN,
    UNIT_CUBIC_METER,
)

from . import Kamstrup, CONF_KAMSTRUP_ID

CONF___REGISTER__ = "__register__"


def sensor_schema(**kwargs) -> cv.Schema:
    reg = kwargs.pop("register")
    return sensor.sensor_schema(**kwargs).extend(
        {
            cv.Optional(CONF___REGISTER__, default=reg): cv.uint16_t,
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KAMSTRUP_ID): cv.use_id(Kamstrup),
        cv.Optional("energy"): sensor_schema(
            unit_of_measurement="GJ",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            register=60,  # Heat energy
        ),
        cv.Optional("power"): sensor_schema(
            unit_of_measurement="GJ",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            register=80,  # Current Power
        ),
        cv.Optional("forward_temp"): sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            register=86,  # Current Forward Temperature
        ),
        cv.Optional("return_temp"): sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            register=87,  # Current Return Temperature
        ),
        cv.Optional("differencial_temp"): sensor_schema(
            unit_of_measurement=UNIT_KELVIN,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            register=89,  # Current Differencial Temperature
        ),
        cv.Optional("flow"): sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLUME,
            state_class=STATE_CLASS_MEASUREMENT,
            register=74,  # Current Flow
        ),
        cv.Optional("volume"): sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLUME,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            register=68,  # Volume
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_KAMSTRUP_ID])

    for _, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf[CONF_ID]
        if id and id.type == sensor.Sensor:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_sensor(conf[CONF___REGISTER__], sens))
