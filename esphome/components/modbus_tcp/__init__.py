import esphome.codegen as cg
from esphome.components import binary_sensor, socket, tcp_uart, uart
from esphome.components.const import (
    CONF_ALLOWED_IPS,
    CONF_CONNECTED,
    CONF_DATA_BITS,
    CONF_HOST,
    CONF_PARITY,
    CONF_RECONNECT_INTERVAL,
    CONF_ROLE,
    CONF_STOP_BITS,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_PORT,
    CONF_TIMEOUT,
    CONF_UART_ID,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["network", "uart"]
AUTO_LOAD = ["socket", "binary_sensor"]
MULTI_CONF = True

CONF_TCP_UART_ID = "tcp_uart_id"
CONF_SEND_WAIT_TIME = "send_wait_time"

modbus_tcp_ns = cg.esphome_ns.namespace("modbus_tcp")
ModbusTcp = modbus_tcp_ns.class_("ModbusTcp", uart.UARTComponent, cg.Component)
ModbusTcpUart = modbus_tcp_ns.class_("ModbusTcpUart", cg.Component, uart.UARTDevice)

LINK_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTcp),
        cv.Required(CONF_TCP_UART_ID): cv.use_id(tcp_uart.TcpUart),
    }
).extend(cv.COMPONENT_SCHEMA)

GATEWAY_BASE = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTcpUart),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_PORT): cv.port,
        cv.Optional(
            CONF_RECONNECT_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SEND_WAIT_TIME, default="2s"): cv.All(
            cv.positive_not_null_time_period, cv.positive_time_period_milliseconds
        ),
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

GATEWAY_SCHEMA = cv.typed_schema(
    {
        "client": GATEWAY_BASE.extend({cv.Required(CONF_HOST): cv.string}),
        "server": GATEWAY_BASE.extend(
            {cv.Optional(CONF_ALLOWED_IPS): socket.IPV4_ALLOW_SCHEMA}
        ),
    },
    key=CONF_ROLE,
    default_type="client",
    lower=True,
)


def _validate(config: ConfigType) -> ConfigType:
    if CONF_TCP_UART_ID in config and CONF_UART_ID in config:
        raise cv.Invalid("set tcp_uart_id or uart_id, not both")
    if CONF_TCP_UART_ID in config:
        return LINK_SCHEMA(config)
    if CONF_UART_ID in config:
        return socket.consume_role_sockets("modbus_tcp")(GATEWAY_SCHEMA(config))
    raise cv.Invalid("tcp_uart_id or uart_id is required")


CONFIG_SCHEMA = _validate


def _final_validate(config: ConfigType) -> None:
    full = fv.full_config.get()
    hubs = (full.get("modbus") or []) if full is not None else []
    # The link is a UART a client hub can sit on. A server hub writes a reply,
    # and the link would send that reply out as a new request.
    if CONF_TCP_UART_ID in config:
        link_id = str(config[CONF_ID])
        for hub in hubs:
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
        return
    # role server forwards a TCP query to the device on this UART. A hub on the
    # same UART would answer, or ask, in place of that device.
    bus = str(config.get(CONF_UART_ID, ""))
    for hub in hubs:
        if bus == "" or str(hub.get(CONF_UART_ID, "")) != bus:
            continue
        if hub.get(CONF_ROLE, "client") == "server":
            raise cv.Invalid(
                "A modbus hub with role: server does not answer a TCP query on this UART. "
                "modbus_tcp role server forwards the query to the device on these pins "
                "and returns that device's reply",
                [CONF_UART_ID],
            )
        raise cv.Invalid(
            "A modbus hub cannot share this UART. modbus_tcp owns it and forwards "
            "between the socket and the pins",
            [CONF_UART_ID],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_TCP_UART_ID in config:
        cg.add_define("USE_MODBUS_TCP_LINK")
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
        return
    cg.add_define("USE_MODBUS_TCP_UART")
    await uart.register_uart_device(var, config)
    if config[CONF_ROLE] == "server":
        socket.require_tcp_listener()
        cg.add(var.set_server(True))
        socket.add_ipv4_allow(
            var.set_allow, config.get(CONF_ALLOWED_IPS), config[CONF_ID]
        )
    else:
        socket.require_tcp_client_link()
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_send_wait_time(config[CONF_SEND_WAIT_TIME]))
    if (host := config.get(CONF_HOST)) is not None:
        cg.add(var.set_host(host))
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CONNECTED, var.set_connected_sensor)
