import esphome.codegen as cg
from esphome.components import binary_sensor, socket, uart
from esphome.components.const import CONF_DATA_BITS, CONF_HOST, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_PORT,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["network", "socket"]
AUTO_LOAD = ["uart", "binary_sensor", "socket"]
MULTI_CONF = True

tcp_uart_ns = cg.esphome_ns.namespace("tcp_uart")
TcpUart = tcp_uart_ns.class_("TcpUart", uart.UARTComponent, cg.Component)

CONF_RECONNECT_INTERVAL = "reconnect_interval"
CONF_CONNECTED = "connected"


def _consume_socket(config: ConfigType) -> ConfigType:
    socket.consume_sockets(1, "tcp_uart")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TcpUart),
            cv.Required(CONF_HOST): cv.string,
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
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _consume_socket,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    # The socket is not clocked. These only satisfy UARTComponent and a consumer check.
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))
    cg.add(var.set_rx_buffer_size(1024))
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CONNECTED, var.set_connected_sensor)
