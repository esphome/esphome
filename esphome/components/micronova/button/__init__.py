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

CONF_TEMPERATURE_UP = "temperature_up"
CONF_TEMPERATURE_DOWN = "temperature_down"
CONF_MEMORY_DATA = "memory_data"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_TEMPERATURE_UP): button.button_schema(
            MicroNovaButton,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend({cv.Optional(CONF_MEMORY_DATA, default=0x00): cv.hex_int_range()}),
        cv.Optional(CONF_TEMPERATURE_DOWN): button.button_schema(
            MicroNovaButton,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend({cv.Optional(CONF_MEMORY_DATA, default=0x00): cv.hex_int_range()}),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if temperature_up_config := config.get(CONF_TEMPERATURE_UP):
        bt = await button.new_button(temperature_up_config, mv)
        cg.add(
            bt.set_memory_location(
                temperature_up_config.get(CONF_MEMORY_LOCATION, 0xA0)
            )
        )
        cg.add(
            bt.set_memory_address(temperature_up_config.get(CONF_MEMORY_ADDRESS, 0x7D))
        )
        cg.add(bt.set_memory_data(temperature_up_config[CONF_MEMORY_DATA]))
        cg.add(bt.set_function(MicroNovaFunctions.STOVE_FUNCTION_TEMP_UP))

    if temperature_down_config := config.get(CONF_TEMPERATURE_DOWN):
        bt = await button.new_button(temperature_down_config, mv)
        cg.add(
            bt.set_memory_location(
                temperature_down_config.get(CONF_MEMORY_LOCATION, 0xA0)
            )
        )
        cg.add(
            bt.set_memory_address(
                temperature_down_config.get(CONF_MEMORY_ADDRESS, 0x7D)
            )
        )
        cg.add(bt.set_memory_data(temperature_down_config[CONF_MEMORY_DATA]))
        cg.add(bt.set_function(MicroNovaFunctions.STOVE_FUNCTION_TEMP_DOWN))
