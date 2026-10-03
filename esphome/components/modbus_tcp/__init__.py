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
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> None:
    # A server hub writes its RTU reply here. This class would send that reply
    # out as a new TCP request with a new transaction id.
    full = fv.full_config.get()
    link_id = str(config[CONF_ID])
    for hub in (full.get("modbus") or []) if full is not None else []:
        if str(hub.get(CONF_UART_ID, "")) != link_id:
            continue
        if hub.get(CONF_ROLE, "client") != "server":
            continue
        raise cv.Invalid(
            "A modbus hub with role: server writes a reply. This link sends that "
            "write out as a new Modbus TCP request, and it only delivers a TCP "
            "response that matches its own request. Use role: client. A TCP client "
            "is answered by modbus_tcp with role: server on the hardware UART, "
            "which forwards the query to the pins",
            [CONF_ID],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_TCP_UART_ID])
    cg.add(var.set_parent(parent))
    # The socket is not clocked. The hub reads these from its UART during
    # setup(), and every cg.add() runs before App.setup().
    parent_config = CORE.config.get_config_for_path(
        CORE.config.get_path_for_id(config[CONF_TCP_UART_ID])[:-1]
    )
    cg.add(var.set_baud_rate(parent_config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(parent_config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(parent_config[CONF_STOP_BITS]))
    cg.add(var.set_parity(parent_config[CONF_PARITY]))
