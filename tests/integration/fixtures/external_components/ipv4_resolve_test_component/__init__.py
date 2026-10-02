import esphome.codegen as cg
from esphome.components import socket
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["socket"]

ipv4_resolve_test_component_ns = cg.esphome_ns.namespace("ipv4_resolve_test_component")
Ipv4ResolveTestComponent = ipv4_resolve_test_component_ns.class_(
    "Ipv4ResolveTestComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Ipv4ResolveTestComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    socket.require_ipv4_resolve()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
