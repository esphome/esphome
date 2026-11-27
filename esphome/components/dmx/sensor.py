import esphome.codegen as cg
from esphome.components import sensor, number
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    ICON_BRIGHTNESS_5,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
)
import esphome.final_validate as fv

from . import (
    CONF_DMX_ID,
    CONF_MODE,
    MODE_RX,
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
    hub_config = fv.get_declaring_config(config[CONF_DMX_ID])

    if hub_config.get(CONF_MODE, MODE_RX) != MODE_RX:
        raise cv.Invalid("dmx sensor requires hub mode 'rx', not 'tx'")

    if CONF_CHANNEL_ID in config:
        number_config = fv.get_declaring_config(config[CONF_CHANNEL_ID])

        min_value = number_config.get("min_value")
        max_value = number_config.get("max_value")
        step = number_config.get("step")

        if min_value != 1:
            raise cv.Invalid(
                "channel_id Number must have min_value: 1",
                [CONF_CHANNEL_ID],
            )

        if max_value != 512:
            raise cv.Invalid(
                "channel_id Number must have max_value: 512",
                [CONF_CHANNEL_ID],
            )

        if step != 1:
            raise cv.Invalid(
                "channel_id Number must have step: 1",
                [CONF_CHANNEL_ID],
            )

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
