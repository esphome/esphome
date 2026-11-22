"""Sensors published by the analog_gauge_reader hub."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_ID,
    CONF_VALUE,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_CONFIDENCE, CONF_DIALS, AnalogGaugeReader

DEPENDENCIES = ["analog_gauge_reader"]

CONF_ANALOG_GAUGE_READER_ID = "analog_gauge_reader_id"
CONF_DIAL = "dial"
_KEY_DIAL_INDEX = "_analog_gauge_reader_dial_index"

VALUE_SCHEMA = sensor.sensor_schema(state_class=STATE_CLASS_MEASUREMENT)

DIAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DIAL): cv.string,
        cv.Optional(CONF_VALUE): VALUE_SCHEMA,
        cv.Optional(CONF_CONFIDENCE): sensor.sensor_schema(
            accuracy_decimals=2, state_class=STATE_CLASS_MEASUREMENT
        ),
        cv.Optional(CONF_ANGLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ANALOG_GAUGE_READER_ID): cv.use_id(AnalogGaugeReader),
        cv.Optional(CONF_VALUE): VALUE_SCHEMA,
        cv.Optional(CONF_DIALS): cv.ensure_list(DIAL_SCHEMA),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    """Match every dial entry to its position in the hub's dial list."""
    hub = fv.get_declaring_config(config[CONF_ANALOG_GAUGE_READER_ID])
    # The hub rejects duplicate dial ids, so each name has exactly one position.
    index_by_id = {dial[CONF_ID]: index for index, dial in enumerate(hub[CONF_DIALS])}
    names = [entry[CONF_DIAL] for entry in config.get(CONF_DIALS, [])]
    if len(set(names)) != len(names):
        raise cv.Invalid("A dial can have only one sensor entry", path=[CONF_DIALS])
    for entry in config.get(CONF_DIALS, []):
        if (index := index_by_id.get(entry[CONF_DIAL])) is None:
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
