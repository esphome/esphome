import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.components.ota import OTAComponent
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_INVERTED,
    CONF_ICON,
    CONF_OTA,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import safe_mode_ns

DEPENDENCIES = ["ota"]

SafeModeSwitch = safe_mode_ns.class_("SafeModeSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SafeModeSwitch),
        cv.GenerateID(CONF_OTA): cv.use_id(OTAComponent),
        cv.Optional(CONF_INVERTED): cv.invalid(
            "Safe Mode Restart switches do not support inverted mode!"
        ),
        cv.Optional(CONF_ICON, default=ICON_RESTART_ALERT): switch.icon,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
        ): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    ota = await cg.get_variable(config[CONF_OTA])
    cg.add(var.set_ota(ota))
