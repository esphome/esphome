import esphome.codegen as cg
from esphome.components import tcp_uart, uart
from esphome.components.const import (
    CONF_DATA_BITS,
    CONF_PARITY,
    CONF_ROLE,
    CONF_STOP_BITS,
)
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_UART_ID
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["tcp_uart"]
MULTI_CONF = True

CONF_TCP_UART_ID = "tcp_uart_id"

modbus_tcp_ns = cg.esphome_ns.namespace("modbus_tcp")
ModbusTcp = modbus_tcp_ns.class_("ModbusTcp", uart.UARTComponent, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTcp),
        cv.Required(CONF_TCP_UART_ID): cv.use_id(tcp_uart.TcpUart),
        cv.Optional(CONF_ROLE, default="client"): cv.one_of(
            "client", "server", lower=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> None:
    # The hub's write is a request in one role and a reply in the other.
    full = fv.full_config.get()
    link_id = str(config[CONF_ID])
    want = config.get(CONF_ROLE, "client")
    for hub in (full.get("modbus") or []) if full is not None else []:
        if str(hub.get(CONF_UART_ID, "")) != link_id:
            continue
        if hub.get(CONF_ROLE, "client") == want:
            continue
        raise cv.Invalid(
            f"The modbus hub on this link must use role: {want}. A client link "
            "sends the hub's write as a new request. A server link sends it as "
            "the reply, with the request's transaction id",
            [CONF_ROLE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_TCP_UART_ID])
    cg.add(var.set_parent(parent))
    if config[CONF_ROLE] == "server":
        cg.add(var.set_server(True))
    # The socket is not clocked. The hub reads these from its UART during
    # setup(), and every cg.add() runs before App.setup().
    parent_config = CORE.config.get_config_for_path(
        CORE.config.get_path_for_id(config[CONF_TCP_UART_ID])[:-1]
    )
    cg.add(var.set_baud_rate(parent_config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(parent_config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(parent_config[CONF_STOP_BITS]))
    cg.add(var.set_parity(parent_config[CONF_PARITY]))
