import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinSwitch = sendspin_ns.class_("SendspinSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(
        SendspinSwitch,
        block_inverted=True,
        default_restore_mode="RESTORE_DEFAULT_ON",
        entity_category=ENTITY_CATEGORY_CONFIG,
    )
    .extend({cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
