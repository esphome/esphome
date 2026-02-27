import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from .. import (
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    micronova_ns,
)

MicroNovaButton = micronova_ns.class_("MicroNovaButton", button.Button, cg.Component)

CONF_CUSTOM_BUTTON = "custom_button"
CONF_MEMORY_DATA = "memory_data"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_CUSTOM_BUTTON): button.button_schema(
            MicroNovaButton,
        )
        .extend(
            MICRONOVA_ADDRESS_SCHEMA(
                is_polling_component=False,
            )
        )
        .extend({cv.Required(CONF_MEMORY_DATA): cv.hex_int_range()}),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if custom_button_config := config.get(CONF_CUSTOM_BUTTON):
        bt = await button.new_button(custom_button_config, mv)
        cg.add(bt.set_memory_location(custom_button_config[CONF_MEMORY_LOCATION]))
        cg.add(bt.set_memory_address(custom_button_config[CONF_MEMORY_ADDRESS]))
        cg.add(bt.set_memory_data(custom_button_config[CONF_MEMORY_DATA]))
