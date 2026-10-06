import esphome.codegen as cg
from esphome.components import binary_sensor, socket, uart
from esphome.components.const import (
    CONF_ALLOWED_IPS,
    CONF_CONNECTED,
    CONF_HOST,
    CONF_RECONNECT_INTERVAL,
    CONF_ROLE,
)
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
DEPENDENCIES = ["network", "uart"]
AUTO_LOAD = ["binary_sensor", "socket"]
MULTI_CONF = True

uart_tcp_ns = cg.esphome_ns.namespace("uart_tcp")
UartTcp = uart_tcp_ns.class_("UartTcp", cg.Component, uart.UARTDevice)


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UartTcp),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_PORT): cv.port,
        cv.Optional(
            CONF_RECONNECT_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "client": BASE_SCHEMA.extend({cv.Required(CONF_HOST): socket.ipv4_host}),
            "server": BASE_SCHEMA.extend(
                {cv.Optional(CONF_ALLOWED_IPS): socket.IPV4_ALLOW_SCHEMA}
            ),
        },
        key=CONF_ROLE,
        default_type="client",
        lower=True,
    ),
    socket.consume_role_sockets("uart_tcp"),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
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
    if (host := config.get(CONF_HOST)) is not None:
        cg.add(var.set_host(host))
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CONNECTED, var.set_connected_sensor)
