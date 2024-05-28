import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["network"]


def AUTO_LOAD():
    if CORE.is_esp8266 or CORE.is_rp2040:
        return []
    return ["socket"]


CONF_TARGET_MAC_ADDRESS = "target_mac_address"

wake_on_lan_ns = cg.esphome_ns.namespace("wake_on_lan")

WakeOnLanButton = wake_on_lan_ns.class_("WakeOnLanButton", button.Button, cg.Component)

CONFIG_SCHEMA = (
    button.button_schema(WakeOnLanButton)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TARGET_MAC_ADDRESS): cv.mac_address,
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_macaddr(*config[CONF_TARGET_MAC_ADDRESS].parts))
    await cg.register_component(var, config)
    await button.register_button(var, config)
