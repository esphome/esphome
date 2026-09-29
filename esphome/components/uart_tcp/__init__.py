import esphome.codegen as cg
from esphome.components import binary_sensor, socket, uart
from esphome.components.const import CONF_HOST
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_UART_ID,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["network", "socket", "uart"]
AUTO_LOAD = ["binary_sensor", "socket"]
MULTI_CONF = True

uart_tcp_ns = cg.esphome_ns.namespace("uart_tcp")
UartTcp = uart_tcp_ns.class_("UartTcp", cg.Component, uart.UARTDevice)

CONF_ROLE = "role"
CONF_RECONNECT_INTERVAL = "reconnect_interval"
CONF_CONNECTED = "connected"


def _validate(config: ConfigType) -> ConfigType:
    if config[CONF_ROLE] == "server" and CONF_HOST in config:
        raise cv.Invalid("host is only used when role is client", path=[CONF_HOST])
    if config[CONF_ROLE] == "client" and CONF_HOST not in config:
        raise cv.Invalid("host is required when role is client", path=[CONF_HOST])
    listens = 1 if config[CONF_ROLE] == "server" else 0
    if listens:
        socket.consume_sockets(1, "uart_tcp", socket.SocketType.TCP_LISTEN)(config)
    socket.consume_sockets(1, "uart_tcp")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UartTcp),
            cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_ROLE, default="client"): cv.one_of(
                "client", "server", lower=True
            ),
            cv.Optional(CONF_HOST): cv.string,
            cv.Required(CONF_PORT): cv.port,
            cv.Optional(
                CONF_RECONNECT_INTERVAL, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_server(config[CONF_ROLE] == "server"))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    if (host := config.get(CONF_HOST)) is not None:
        cg.add(var.set_host(host))
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CONNECTED, var.set_connected_sensor)
