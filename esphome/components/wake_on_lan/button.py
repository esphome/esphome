import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID

CONF_TARGET_MAC_ADDRESS = "target_mac_address"

wake_on_lan_ns = cg.esphome_ns.namespace("wake_on_lan")

WakeOnLanButton = wake_on_lan_ns.class_("WakeOnLanButton", button.Button, cg.Component)

DEPENDENCIES = ["wifi"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_TARGET_MAC_ADDRESS): cv.mac_address,
            cv.GenerateID(): cv.declare_id(WakeOnLanButton),
        }
    )
    .extend(button.BUTTON_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(
        var.set_macaddr(
            (int(x, 16) for x in config[CONF_TARGET_MAC_ADDRESS].parts)
        )
    )
    cg.register_component(var, config)
    button.register_button(var, config)
    cg.add(var)
