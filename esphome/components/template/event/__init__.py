from esphome.components import event
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_EVENT_TYPES

from .. import template_ns

CODEOWNERS = ["@nohat"]

TemplateEvent = template_ns.class_("TemplateEvent", event.Event)

CONFIG_SCHEMA = cv.with_visibility(
    event.event_schema(TemplateEvent), cv.Visibility.UI, CONF_DEVICE_CLASS
).extend(
    {
        cv.Required(CONF_EVENT_TYPES): cv.ensure_list(cv.string_strict),
    }
)


async def to_code(config):
    await event.new_event(config, event_types=config[CONF_EVENT_TYPES])
