import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["remote_transmitter"]
AUTO_LOAD = ["remote_base"]
CODEOWNERS = ["@jzucker2"]

CONF_VORNADO_ID = "vornado_id"

vornado_ir_ns = cg.esphome_ns.namespace("vornado_ir")
VornadoIR = vornado_ir_ns.class_(
    "VornadoIR", cg.Component, remote_base.RemoteTransmittable
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VornadoIR),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await remote_base.register_transmittable(var, config)
