import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_CHANNELS, ICON_BRIGHTNESS_5, UNIT_EMPTY

from . import CONF_DMX_ID, DMXComponent

DEPENDENCIES = ["dmx"]

CHANNEL_SCHEMA = number.number_schema(
    number.ApplyNumber,
    unit_of_measurement=UNIT_EMPTY,
    icon=ICON_BRIGHTNESS_5,
).extend(
    {
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=512),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DMX_ID): cv.use_id(DMXComponent),
        cv.Required(CONF_CHANNELS): cv.ensure_list(CHANNEL_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DMX_ID])
    numbers = number.sub_numbers(config, parent=parent)

    for channel_config in config[CONF_CHANNELS]:
        channel = channel_config[CONF_CHANNEL]

        await numbers.create(
            channel_config,
            None,
            apply=f"set_slot({channel}, value)",
            min_value=0,
            max_value=255,
            step=1,
        )

    if config[CONF_CHANNELS]:
        cg.add(parent.enable_tx())
