import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import network
from esphome.const import CONF_ID

usb_ethernet_ns = cg.esphome_ns.namespace("usb_ethernet")
USBEthernetComponent = usb_ethernet_ns.class_(
    "USBEthernetComponent",
    cg.Component,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBEthernetComponent),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if hasattr(network, "register_addressable"):
        await network.register_addressable(var, config)

    cg.add_define("USE_USB_ETHERNET")