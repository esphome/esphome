import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.ota import OTAComponent
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_ICON,
    CONF_OTA,
    CONF_OTA_OPTIONS,
    CONF_SAFE_MODE,
    ICON_RESTART,
)

restart_ns = cg.esphome_ns.namespace("restart")
RestartSwitch = restart_ns.class_("RestartSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RestartSwitch),
        cv.Optional(CONF_INVERTED): cv.invalid(
            "Restart switches do not support inverted mode!"
        ),
        cv.Optional(CONF_ICON, default=ICON_RESTART): switch.icon,
        cv.Optional(CONF_OTA_OPTIONS): cv.Schema(
            {
                cv.GenerateID(CONF_OTA): cv.use_id(OTAComponent),
                cv.Optional(CONF_SAFE_MODE, default=False): cv.boolean,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    if CONF_OTA_OPTIONS in config:
        ota = await cg.get_variable(config[CONF_OTA_OPTIONS][CONF_OTA])
        cg.add(var.set_ota(ota))
        cg.add(var.set_safe_mode(config[CONF_OTA_OPTIONS][CONF_SAFE_MODE]))
