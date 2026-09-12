from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS

from .. import template_ns

TemplateButton = template_ns.class_("TemplateButton", button.Button)

CONFIG_SCHEMA = cv.with_visibility(
    button.button_schema(TemplateButton), cv.Visibility.UI, CONF_DEVICE_CLASS
)


async def to_code(config):
    await button.new_button(config)
