import esphome.config_validation as cv
from esphome.components import button

from .. import template_ns

TemplateButton = template_ns.class_("TemplateButton", button.Button)

CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TemplateButton),
    }
)


async def to_code(config):
    await button.new_button(config)
