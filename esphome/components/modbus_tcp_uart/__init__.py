import esphome.codegen as cg
from esphome.components import modbus, tcp_uart, uart
from esphome.components.const import (
    CONF_DATA_BITS,
    CONF_PARITY,
    CONF_ROLE,
    CONF_STOP_BITS,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BAUD_RATE,
    CONF_DEBUG,
    CONF_ID,
    CONF_UART_ID,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["modbus", "tcp_uart"]
DOMAIN = "modbus_tcp_uart"
MULTI_CONF = True

CONF_REPLY_TIMEOUT = "reply_timeout"
CONF_TCP_UART_ID = "tcp_uart_id"
DEFAULT_REPLY_TIMEOUT_MS = 1000

modbus_tcp_uart_ns = cg.esphome_ns.namespace("modbus_tcp_uart")
ModbusTcpUart = modbus_tcp_uart_ns.class_(
    "ModbusTcpUart", uart.VirtualUARTComponent, cg.Component
)


def _reply_timeout_needs_server(config: ConfigType) -> ConfigType:
    if CONF_REPLY_TIMEOUT in config and config[CONF_ROLE] != "server":
        raise cv.Invalid(
            "reply_timeout is only used with role: server", [CONF_REPLY_TIMEOUT]
        )
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusTcpUart),
            cv.Required(CONF_TCP_UART_ID): cv.use_id(tcp_uart.TcpUart),
            cv.Optional(CONF_ROLE, default="client"): cv.one_of(
                "client", "server", lower=True
            ),
            cv.Optional(CONF_REPLY_TIMEOUT): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=65535)),
            ),
            cv.Optional(CONF_DEBUG): uart.maybe_empty_debug,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .add_extra(_reply_timeout_needs_server)
)


def _final_validate(config: ConfigType) -> None:
    # The hub's write is a request in one role and a reply in the other.
    full = fv.full_config.get()
    link_id = str(config[CONF_ID])
    want = config.get(CONF_ROLE, "client")
    for hub in full.get("modbus") or []:
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


def _served_units(full: ConfigType, link_id: str) -> list[int]:
    """Addresses of the server devices on the hubs that use this link."""
    hubs = {
        str(hub[CONF_ID])
        for hub in full.get("modbus") or []
        if str(hub.get(CONF_UART_ID, "")) == link_id
    }
    return sorted(
        {
            item[CONF_ADDRESS]
            for domain in full.values()
            for item in (domain if isinstance(domain, list) else [domain])
            if isinstance(item, dict)
            and CONF_ADDRESS in item
            and str(item.get(modbus.CONF_MODBUS_ID, "")) in hubs
        }
    )


def _reply_timeout_ms(config: ConfigType, full: ConfigType) -> int:
    """Server: room for one transaction ahead of a forwarded request on a client hub, then its own."""
    if CONF_REPLY_TIMEOUT in config:
        return config[CONF_REPLY_TIMEOUT].total_milliseconds
    waits = [
        hub[modbus.CONF_TURNAROUND_TIME].total_milliseconds
        + hub[modbus.CONF_SEND_WAIT_TIME].total_milliseconds
        for hub in full.get("modbus") or []
        if hub.get(CONF_ROLE, "client") == "client"
        and modbus.CONF_SEND_WAIT_TIME in hub
        and modbus.CONF_TURNAROUND_TIME in hub
    ]
    # One second for the frames and the reply path, as without a client hub.
    return DEFAULT_REPLY_TIMEOUT_MS + 2 * max(waits, default=0)


async def to_code(config: ConfigType) -> None:
    uart.require_virtual_uart()
    parent = await cg.get_variable(config[CONF_TCP_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    if debug := config.get(CONF_DEBUG):
        cg.add_global(uart.uart_ns.using)
        await uart.debug_to_code(debug, var)
    if config[CONF_ROLE] == "server":
        cg.add(var.set_server(True))
        timeout = _reply_timeout_ms(config, CORE.config)
        if timeout != DEFAULT_REPLY_TIMEOUT_MS:
            cg.add(var.set_reply_timeout(timeout))
        if units := _served_units(CORE.config, str(config[CONF_ID])):
            arr = cg.static_const_array(
                ID(f"{config[CONF_ID]}_units", is_declaration=True, type=cg.uint8),
                cg.ArrayInitializer(*units),
            )
            cg.add(var.set_units(arr, len(units)))
    # The reader takes these in setup(); every cg.add() runs before that.
    parent_config = CORE.config.get_config_for_path(
        CORE.config.get_path_for_id(config[CONF_TCP_UART_ID])[:-1]
    )
    cg.add(var.set_baud_rate(parent_config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(parent_config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(parent_config[CONF_STOP_BITS]))
    cg.add(var.set_parity(parent_config[CONF_PARITY]))
