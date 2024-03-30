from esphome.components import event

import esphome.codegen as cg

from .. import template_ns

CODEOWNERS = ["@nohat"]

TemplateEvent = template_ns.class_("TemplateEvent", event.Event, cg.Component)

CONFIG_SCHEMA = event.event_schema(TemplateEvent)


async def to_code(config):
    var = await event.new_event(config)
    await cg.register_component(var, config)
