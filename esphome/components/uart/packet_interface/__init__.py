import esphome.codegen as cg
from esphome.components import packet_interface, uart
from esphome.components.packet_interface import packet_interface_schema
import esphome.config_validation as cv
from esphome.const import CONF_RX_BUFFER_SIZE

from .. import uart_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["uart"]

UartPacketInterface = uart_ns.class_(
    "UartPacketInterface",
    packet_interface.PacketInterface,
    uart.UARTDevice,
    cg.PollingComponent,
)

CONFIG_SCHEMA = (
    packet_interface_schema(UartPacketInterface)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_RX_BUFFER_SIZE, default=1024): cv.int_range(256, 8192),
        }
    )
)


async def to_code(config):
    var = await packet_interface.new_packet_interface(config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
