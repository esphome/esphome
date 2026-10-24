import esphome.codegen as cg
from esphome.components import socket, uart
from esphome.components.const import (
    CONF_ALLOWED_IPS,
    CONF_DATA_BITS,
    CONF_HOST,
    CONF_PARITY,
    CONF_RECONNECT_INTERVAL,
    CONF_ROLE,
    CONF_STOP_BITS,
)
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_MAX_CONNECTIONS, CONF_PORT
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["uart", "socket"]
DOMAIN = "tcp_uart"
MULTI_CONF = True

CONF_TCP_UART_ID = "tcp_uart_id"
CONF_TURN_TIMEOUT = "turn_timeout"

tcp_uart_ns = cg.esphome_ns.namespace("tcp_uart")
TcpUart = tcp_uart_ns.class_("TcpUart", uart.UARTComponent, cg.Component)


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.All(cv.declare_id(TcpUart), uart.mark_unclocked),
        cv.Required(CONF_PORT): cv.port,
        cv.Optional(CONF_BAUD_RATE, default=9600): cv.int_range(min=1),
        cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
        cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
            uart.UART_PARITY_OPTIONS, upper=True
        ),
        cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
        cv.Optional(
            CONF_RECONNECT_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


def _consume_replacement_socket(config: ConfigType) -> ConfigType:
    # A full pool accepts the new client before it closes the quietest one, so for a moment it
    # holds max_connections + 1 sockets; without the spare that accept can fail with ENFILE.
    if config.get(CONF_MAX_CONNECTIONS, 1) > 1:
        socket.consume_sockets(1, "tcp_uart")(config)
    return config


def _validate_turn_timeout(config: ConfigType) -> ConfigType:
    if CONF_TURN_TIMEOUT in config and config.get(CONF_MAX_CONNECTIONS, 1) == 1:
        raise cv.Invalid(
            "turn_timeout needs max_connections of 2 or more", [CONF_TURN_TIMEOUT]
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "client": BASE_SCHEMA.extend(
                {
                    cv.Required(CONF_HOST): socket.ipv4_host,
                }
            ),
            "server": BASE_SCHEMA.extend(
                {
                    cv.Optional(CONF_ALLOWED_IPS): socket.IPV4_ALLOW_SCHEMA,
                    cv.Optional(CONF_MAX_CONNECTIONS, default=1): cv.int_range(
                        min=1, max=8
                    ),
                    # 0 would end every turn before its reply is written.
                    cv.Optional(CONF_TURN_TIMEOUT): cv.All(
                        cv.positive_not_null_time_period,
                        cv.positive_time_period_milliseconds,
                    ),
                }
            ),
        },
        key=CONF_ROLE,
        default_type="client",
        lower=True,
    ),
    _validate_turn_timeout,
    socket.consume_role_sockets("tcp_uart", CONF_MAX_CONNECTIONS),
    _consume_replacement_socket,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if config[CONF_ROLE] == "server":
        socket.require_tcp_listener()
        cg.add(var.set_server(True))
        socket.add_ipv4_allow(
            var.set_allow, config.get(CONF_ALLOWED_IPS), config[CONF_ID]
        )
        if (connections := config[CONF_MAX_CONNECTIONS]) > 1:
            cg.add_define("USE_TCP_UART_MAX_CONNECTIONS")
            cg.add(var.set_max_connections(connections))
            cg.add_optional(var.set_turn_timeout, config.get(CONF_TURN_TIMEOUT))
    else:
        socket.require_tcp_client_link()
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    # The socket is not clocked. These only satisfy UARTComponent and a consumer check.
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))
    cg.add_optional(var.set_host, config.get(CONF_HOST))
