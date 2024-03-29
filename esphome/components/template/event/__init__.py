from esphome.components import event

from .. import template_ns

import esphome.codegen as cg

TemplateEvent = template_ns.class_("TemplateEvent", event.Event, cg.Component)

CONFIG_SCHEMA = event.event_schema(TemplateEvent)


async def to_code(config):
    var = await event.new_event(config)
    await cg.register_component(var, config)
