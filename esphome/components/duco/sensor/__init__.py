from typing import NamedTuple

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_SECOND,
)
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

UNIT_DAYS = "d"

CONF_BYPASS = "bypass"
CONF_FILTER_REMAINING = "filter_remaining"
CONF_FLOW_LEVEL = "flow_level"
CONF_TEMPERATURE_EHA = "temperature_eha"
CONF_TEMPERATURE_ETA = "temperature_eta"
CONF_TEMPERATURE_ODA = "temperature_oda"
CONF_TEMPERATURE_SUP = "temperature_sup"
CONF_TIME_REMAINING = "time_remaining"

FUNCTION_NODE = 0x0C
FUNCTION_NODE_DATA = 0x10
FUNCTION_PARAMETER = 0x24

DucoSensor = duco_ns.class_("DucoSensor", cg.PollingComponent, sensor.Sensor)
DucoValueFormat = duco_ns.enum("DucoValueFormat", is_class=True)
UINT8 = DucoValueFormat.DUCO_VALUE_FORMAT_UINT8
UINT16 = DucoValueFormat.DUCO_VALUE_FORMAT_UINT16
INT16 = DucoValueFormat.DUCO_VALUE_FORMAT_INT16


class Reading(NamedTuple):
    """Where a value is in the box's answer; argument None means the configured node address."""

    function: int
    argument: int | None
    offset: int
    value_format: cg.MockObj
    divisor: int
    minimum: int
    maximum: int


# The box reports out of range values for sensors that are missing or still starting
READINGS = {
    CONF_CO2: Reading(FUNCTION_NODE_DATA, None, 4, UINT16, 1, 300, 10000),
    CONF_TEMPERATURE: Reading(FUNCTION_NODE_DATA, None, 2, UINT16, 10, 0, 1000),
    CONF_HUMIDITY: Reading(FUNCTION_NODE_DATA, None, 6, UINT16, 100, 0, 10000),
    CONF_FILTER_REMAINING: Reading(FUNCTION_PARAMETER, 0x30, 3, UINT8, 1, 0, 255),
    CONF_FLOW_LEVEL: Reading(FUNCTION_NODE, 0x01, 2, UINT8, 1, 0, 255),
    CONF_TIME_REMAINING: Reading(FUNCTION_NODE, 0x01, 12, UINT16, 1, 0, 65535),
    CONF_TEMPERATURE_ODA: Reading(FUNCTION_PARAMETER, 0x00, 3, INT16, 10, -999, 1000),
    CONF_TEMPERATURE_SUP: Reading(FUNCTION_PARAMETER, 0x01, 3, INT16, 10, -999, 1000),
    CONF_TEMPERATURE_ETA: Reading(FUNCTION_PARAMETER, 0x02, 3, INT16, 10, -999, 1000),
    CONF_TEMPERATURE_EHA: Reading(FUNCTION_PARAMETER, 0x03, 3, INT16, 10, -999, 1000),
    CONF_BYPASS: Reading(FUNCTION_PARAMETER, 0x10, 3, UINT8, 1, 0, 100),
}


def _schema(**kwargs) -> cv.Schema:
    return sensor.sensor_schema(
        DucoSensor, state_class=STATE_CLASS_MEASUREMENT, **kwargs
    ).extend(cv.polling_component_schema("60s"))


def _node_schema(**kwargs) -> cv.All:
    return cv.ensure_list(
        _schema(**kwargs).extend({cv.Required(CONF_ADDRESS): cv.int_range(0, 68)})
    )


_TEMPERATURE = {
    "unit_of_measurement": UNIT_CELSIUS,
    "accuracy_decimals": 1,
    "device_class": DEVICE_CLASS_TEMPERATURE,
}

CONFIG_SCHEMA = DUCO_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_CO2): _node_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
        ),
        cv.Optional(CONF_TEMPERATURE): _node_schema(**_TEMPERATURE),
        cv.Optional(CONF_HUMIDITY): _node_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
        ),
        cv.Optional(CONF_FILTER_REMAINING): _schema(
            unit_of_measurement=UNIT_DAYS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
        ).extend(cv.polling_component_schema("1200s")),
        cv.Optional(CONF_FLOW_LEVEL): _schema(
            unit_of_measurement=UNIT_PERCENT, accuracy_decimals=0
        ),
        cv.Optional(CONF_TIME_REMAINING): _schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
        ),
        cv.Optional(CONF_TEMPERATURE_ODA): _schema(**_TEMPERATURE),
        cv.Optional(CONF_TEMPERATURE_SUP): _schema(**_TEMPERATURE),
        cv.Optional(CONF_TEMPERATURE_ETA): _schema(**_TEMPERATURE),
        cv.Optional(CONF_TEMPERATURE_EHA): _schema(**_TEMPERATURE),
        cv.Optional(CONF_BYPASS): _schema(
            unit_of_measurement=UNIT_PERCENT, accuracy_decimals=0
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    sensors = sensor.sub_sensors(config, parent=hub)
    for key, reading in READINGS.items():
        if (confs := config.get(key)) is None:
            continue
        # Node sensors are lists, one entry per node address
        for conf in confs if reading.argument is None else [confs]:
            argument = (
                conf[CONF_ADDRESS] if reading.argument is None else reading.argument
            )
            var = await sensors.create(
                conf,
                None,
                reading.function,
                argument,
                reading.offset,
                reading.value_format,
                reading.divisor,
                reading.minimum,
                reading.maximum,
            )
            await cg.register_component(var, conf)
