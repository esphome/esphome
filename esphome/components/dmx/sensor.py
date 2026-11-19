import esphome.codegen as cg
from esphome.components import number, sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, ICON_BRIGHTNESS_5, STATE_CLASS_MEASUREMENT, UNIT_EMPTY
import esphome.final_validate as fv

from . import (
    CONF_DMX_ID,
    CONF_MODE,
    MODE_TX,
    DMXComponent,
    register_channel_sensor,
    register_dynamic_channel_sensor,
)

DEPENDENCIES = ["dmx"]
CONF_CHANNEL_ID = "channel_id"

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_BRIGHTNESS_5,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.GenerateID(CONF_DMX_ID): cv.use_id(DMXComponent),
            cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=512),
            cv.Optional(CONF_CHANNEL_ID): cv.use_id(number.Number),
        }
    ),
    cv.has_exactly_one_key(CONF_CHANNEL, CONF_CHANNEL_ID),
)


def _final_validate(config):
    full = fv.full_config.get()
    hubs = full.get("dmx", [])
    if not isinstance(hubs, list):
        hubs = [hubs] if hubs else []
    target = config[CONF_DMX_ID].id
    for hub in hubs:
        if hub[CONF_ID].id == target:
            if hub.get(CONF_MODE, "rx") == MODE_TX:
                raise cv.Invalid("dmx sensor requires hub mode 'rx' or 'bidirectional', not 'tx'")
            break
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_DMX_ID])
    if CONF_CHANNEL in config:
        cg.add(register_channel_sensor(parent, config[CONF_CHANNEL], var))
    else:
        channel_number = await cg.get_variable(config[CONF_CHANNEL_ID])
        cg.add(register_dynamic_channel_sensor(parent, channel_number, var))
