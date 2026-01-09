import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_RESTART

restart_ns = cg.esphome_ns.namespace("restart")
RestartSwitch = restart_ns.class_("RestartSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(
    RestartSwitch,
    icon=ICON_RESTART,
    entity_category=ENTITY_CATEGORY_CONFIG,
    block_inverted=True,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
