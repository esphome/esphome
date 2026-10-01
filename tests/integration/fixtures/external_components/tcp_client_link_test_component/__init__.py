import esphome.codegen as cg
from esphome.components.const import CONF_HOST
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.types import ConfigType

AUTO_LOAD = ["socket"]

CONF_RECONNECT_INTERVAL = "reconnect_interval"

tcp_client_link_test_component_ns = cg.esphome_ns.namespace(
    "tcp_client_link_test_component"
)
TcpClientLinkTestComponent = tcp_client_link_test_component_ns.class_(
    "TcpClientLinkTestComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TcpClientLinkTestComponent),
        cv.Required(CONF_HOST): cv.string,
        cv.Required(CONF_PORT): cv.port,
        cv.Optional(
            CONF_RECONNECT_INTERVAL, default="1s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
