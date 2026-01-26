import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@clydebarrow"]
IS_PLATFORM_COMPONENT = True

transport_ns = cg.esphome_ns.namespace("transport")
Transport = transport_ns.class_("Transport", cg.Component)

CONF_TRANSPORT = "transport"


def transport_schema(
    class_: MockObjClass,
):
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    )


async def new_transport(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)
    return var
