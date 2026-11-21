"""Sensors published by the analog_gauge_reader hub."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ANGLE, CONF_ID, CONF_VALUE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_CONFIDENCE, CONF_DIALS, AnalogGaugeReader

DEPENDENCIES = ["analog_gauge_reader"]

CONF_ANALOG_GAUGE_READER_ID = "analog_gauge_reader_id"
CONF_DIAL = "dial"
_KEY_DIAL_INDEX = "_analog_gauge_reader_dial_index"

DIAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DIAL): cv.string,
        cv.Optional(CONF_VALUE): sensor.sensor_schema(),
        cv.Optional(CONF_CONFIDENCE): sensor.sensor_schema(),
        cv.Optional(CONF_ANGLE): sensor.sensor_schema(),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ANALOG_GAUGE_READER_ID): cv.use_id(AnalogGaugeReader),
        cv.Optional(CONF_VALUE): sensor.sensor_schema(),
        cv.Optional(CONF_DIALS): cv.ensure_list(DIAL_SCHEMA),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    """Match every dial entry to its position in the hub's dial list."""
    hub = fv.get_declaring_config(config[CONF_ANALOG_GAUGE_READER_ID])
    indices = {dial[CONF_ID]: index for index, dial in enumerate(hub[CONF_DIALS])}
    for entry in config.get(CONF_DIALS, []):
        if (index := indices.get(entry[CONF_DIAL])) is None:
            raise cv.Invalid(
                f"analog_gauge_reader has no dial '{entry[CONF_DIAL]}'",
                path=[CONF_DIALS],
            )
        entry[_KEY_DIAL_INDEX] = index
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ANALOG_GAUGE_READER_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VALUE, hub.set_value_sensor)
    for entry in config.get(CONF_DIALS, []):
        index = entry[_KEY_DIAL_INDEX]
        if (value := entry.get(CONF_VALUE)) is not None:
            sens = await sensor.new_sensor(value)
            cg.add(hub.set_dial_value_sensor(index, sens))
        if (confidence := entry.get(CONF_CONFIDENCE)) is not None:
            sens = await sensor.new_sensor(confidence)
            cg.add(hub.set_dial_confidence_sensor(index, sens))
        if (angle := entry.get(CONF_ANGLE)) is not None:
            sens = await sensor.new_sensor(angle)
            cg.add(hub.set_dial_angle_sensor(index, sens))
