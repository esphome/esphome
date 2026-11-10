import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, number
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    ICON_BRIGHTNESS_5,
    UNIT_EMPTY,
    CONF_CHANNEL,
)
from . import DMXReader, dmx_reader_ns

DEPENDENCIES = ["dmx_reader"]

CONF_CHANNEL_ID = "channel_id"

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_BRIGHTNESS_5,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(sensor.Sensor),
            cv.Required("dmx_reader_id"): cv.use_id(DMXReader),
            # Of vast kanaal, of dynamisch via number
            cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=512),
            cv.Optional(CONF_CHANNEL_ID): cv.use_id(number.Number),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_CHANNEL, CONF_CHANNEL_ID),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config["dmx_reader_id"])

    if CONF_CHANNEL in config:
        # Vast kanaal
        cg.add(parent.register_channel_sensor(config[CONF_CHANNEL], var))
    else:
        # Dynamisch kanaal via Number-entity
        channel_number = await cg.get_variable(config[CONF_CHANNEL_ID])
        cg.add(parent.register_dynamic_channel_sensor(channel_number, var))
