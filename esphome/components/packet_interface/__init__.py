import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@clydebarrow"]
IS_PLATFORM_COMPONENT = True

packet_interface_ns = cg.esphome_ns.namespace("packet_interface")
PacketInterface = packet_interface_ns.class_("PacketInterface", cg.Component)

CONF_PACKET_INTERFACE = "packet_interface"
CONF_MAX_PACKET_SIZE = "max_packet_size"


def packet_interface_schema(
    class_: MockObjClass,
):
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.Optional(CONF_MAX_PACKET_SIZE): cv.int_range(16, 4096),
        }
    )


async def new_packet_interface(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    if max_packet_size := config.get(CONF_MAX_PACKET_SIZE):
        cg.add(var.set_max_packet_size(max_packet_size))
    await cg.register_component(var, config)
    return var
