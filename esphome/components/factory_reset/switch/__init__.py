import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_RESTART_ALERT

from .. import factory_reset_ns

FactoryResetSwitch = factory_reset_ns.class_(
    "FactoryResetSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(
    FactoryResetSwitch,
    block_inverted=True,
    icon=ICON_RESTART_ALERT,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
