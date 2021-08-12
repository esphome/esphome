from esphome.const import CONF_ID
import esphome.config_validation as cv
import esphome.codegen as cg

AUTO_LOAD = ["socket"]

echo_ns = cg.esphome_ns.namespace("echo")
EchoServer = echo_ns.class_("EchoServer", cg.Component)
EchoNoiseServer = echo_ns.class_("EchoNoiseServer", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional("ssl"): cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(EchoServer),
            }
        ),
        cv.Optional("noise"): cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(EchoNoiseServer),
            }
        ),
    }
)


async def to_code(config):
    conf = config.get("ssl")
    if conf is not None:
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        cg.add_define("USE_ECHO_SSL")
    conf = config.get("noise")
    if conf is not None:
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        cg.add_define("USE_ECHO_NOISE")
        cg.add_library("esphome/noise-c", "0.1.0")
