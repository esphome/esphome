import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@clydebarrow"]
IS_PLATFORM_COMPONENT = True

packet_interface_ns = cg.esphome_ns.namespace("packet_interface")
PacketInterface = packet_interface_ns.class_("PacketInterface", cg.Component)

CONF_PACKET_INTERFACE = "packet_interface"


def packet_interface_schema(
    class_: MockObjClass,
):
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    )


async def new_packet_interface(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)
    return var
