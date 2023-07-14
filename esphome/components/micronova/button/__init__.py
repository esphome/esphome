import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import (
    MicroNova,
    MicroNovaFunctions,
    CONF_MICRONOVA_ID,
    CONF_MEMORY_LOCATION,
    CONF_MEMORY_ADDRESS,
    MICRONOVA_LISTENER_SCHEMA,
    micronova_ns,
)

MicroNovaButton = micronova_ns.class_("MicroNovaButton", button.Button, cg.Component)

CONF_BUT_TEMP_UP = "but_temp_up"
CONF_BUT_TEMP_DOWN = "but_temp_down"
CONF_MEMORY_DATA = "memory_data"

TYPES = [
    CONF_BUT_TEMP_UP,
    CONF_BUT_TEMP_DOWN,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_BUT_TEMP_UP): button.button_schema(
            MicroNovaButton,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend({cv.Optional(CONF_MEMORY_DATA, default=0x00): cv.hex_int_range()}),
        cv.Optional(CONF_BUT_TEMP_DOWN): button.button_schema(
            MicroNovaButton,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend({cv.Optional(CONF_MEMORY_DATA, default=0x00): cv.hex_int_range()}),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])
    for key in TYPES:
        if key in config:
            conf = config[key]
            bt = await button.new_button(conf, mv)
            cg.add(bt.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0xA0)))
            cg.add(bt.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x7D)))
            cg.add(bt.set_memory_data(conf[CONF_MEMORY_DATA]))
            if key == CONF_BUT_TEMP_UP:
                cg.add(bt.set_function(MicroNovaFunctions.STOVE_FUNCTION_TEMP_UP))
            if key == CONF_BUT_TEMP_DOWN:
                cg.add(bt.set_function(MicroNovaFunctions.STOVE_FUNCTION_TEMP_DOWN))
