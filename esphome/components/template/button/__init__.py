import esphome.config_validation as cv
from esphome.components import button


CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(button.Button),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    await button.new_button(config)
