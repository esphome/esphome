from esphome.components import button

from .. import template_ns

TemplateButton = template_ns.class_("TemplateButton", button.Button)

CONFIG_SCHEMA = button.button_schema(TemplateButton)


async def to_code(config):
    await button.new_button(config)
