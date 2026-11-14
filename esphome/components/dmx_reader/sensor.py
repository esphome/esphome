import esphome.codegen as cg
from esphome.components import number, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    ICON_BRIGHTNESS_5,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
)

from . import DMXReader, register_channel_sensor, register_dynamic_channel_sensor

DEPENDENCIES = ["dmx_reader"]

CONF_CHANNEL_ID = "channel_id"
CONF_DMX_READER_ID = "dmx_reader_id"

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_BRIGHTNESS_5,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.GenerateID(CONF_DMX_READER_ID): cv.use_id(DMXReader),
            cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=512),
            cv.Optional(CONF_CHANNEL_ID): cv.use_id(number.Number),
        }
    ),
    cv.has_exactly_one_key(CONF_CHANNEL, CONF_CHANNEL_ID),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_DMX_READER_ID])

    if CONF_CHANNEL in config:
        # fixed channel → SlotList (codegen-sized)
        cg.add(register_channel_sensor(parent, config[CONF_CHANNEL], var))
    else:
        # Dynamic channel via Number-entity → SlotList (codegen-sized)
        channel_number = await cg.get_variable(config[CONF_CHANNEL_ID])
        cg.add(register_dynamic_channel_sensor(parent, channel_number, var))
