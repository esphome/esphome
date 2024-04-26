import esphome.config_validation as cv

from esphome.components import event

import esphome.codegen as cg

from esphome.const import CONF_EVENT_TYPES

from .. import template_ns

CODEOWNERS = ["@nohat"]

TemplateEvent = template_ns.class_("TemplateEvent", event.Event, cg.Component)

CONFIG_SCHEMA = event.event_schema(TemplateEvent).extend(
    {
        cv.Required(CONF_EVENT_TYPES): cv.ensure_list(cv.string_strict),
    }
)


async def to_code(config):
    var = await event.new_event(config, event_types=config[CONF_EVENT_TYPES])
    await cg.register_component(var, config)
