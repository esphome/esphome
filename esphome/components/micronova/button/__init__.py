from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    register_micronova_writer,
)

CONF_CUSTOM_BUTTON = "custom_button"
CONF_MEMORY_DATA = "memory_data"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_CUSTOM_BUTTON): button.button_schema(
            button.ApplyButton,
        )
        .extend(
            MICRONOVA_ADDRESS_SCHEMA(
                is_polling_component=False,
            )
        )
        .extend({cv.Required(CONF_MEMORY_DATA): cv.hex_int_range(min=0x00, max=0xFF)}),
    }
)


async def to_code(config: ConfigType) -> None:
    if conf := config.get(CONF_CUSTOM_BUTTON):
        register_micronova_writer()
        location = conf[CONF_MEMORY_LOCATION]
        address = conf[CONF_MEMORY_ADDRESS]
        data = conf[CONF_MEMORY_DATA]
        await button.sub_buttons(config, parent=config[CONF_MICRONOVA_ID]).create(
            conf,
            None,
            apply=f"queue_write_command(0x{location:02X}, 0x{address:02X}, 0x{data:02X})",
        )
