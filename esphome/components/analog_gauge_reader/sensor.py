"""Sensors published by the analog_gauge_reader hub."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ANGLE, CONF_ID, CONF_VALUE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_CONFIDENCE, CONF_DIALS, DOMAIN, AnalogGaugeReader

DEPENDENCIES = ["analog_gauge_reader"]

CONF_ANALOG_GAUGE_READER_ID = f"{DOMAIN}_id"
CONF_DIAL = "dial"
_KEY_DIAL_INDEX = f"_{DOMAIN}_dial_index"

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
    ids = [dial[CONF_ID] for dial in hub[CONF_DIALS]]
    for entry in config.get(CONF_DIALS, []):
        if entry[CONF_DIAL] not in ids:
            raise cv.Invalid(
                f"analog_gauge_reader has no dial '{entry[CONF_DIAL]}'",
                path=[CONF_DIALS],
            )
        entry[_KEY_DIAL_INDEX] = ids.index(entry[CONF_DIAL])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ANALOG_GAUGE_READER_ID])
    if CONF_VALUE in config:
        cg.add(hub.set_value_sensor(await sensor.new_sensor(config[CONF_VALUE])))
    for entry in config.get(CONF_DIALS, []):
        index = entry[_KEY_DIAL_INDEX]
        if CONF_VALUE in entry:
            sens = await sensor.new_sensor(entry[CONF_VALUE])
            cg.add(hub.set_dial_value_sensor(index, sens))
        if CONF_CONFIDENCE in entry:
            sens = await sensor.new_sensor(entry[CONF_CONFIDENCE])
            cg.add(hub.set_dial_confidence_sensor(index, sens))
        if CONF_ANGLE in entry:
            sens = await sensor.new_sensor(entry[CONF_ANGLE])
            cg.add(hub.set_dial_angle_sensor(index, sens))
