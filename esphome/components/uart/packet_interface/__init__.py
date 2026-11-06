import esphome.codegen as cg
from esphome.components import packet_interface, uart
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    packet_interface_schema,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import uart_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["uart"]

CONF_RX_BUFFER_ID = "rx_buffer_id"

UartPacketInterface = uart_ns.class_(
    "UartPacketInterface",
    packet_interface.PacketInterface,
    uart.UARTDevice,
)

CONFIG_SCHEMA = (
    packet_interface_schema(UartPacketInterface, 508)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend({cv.GenerateID(CONF_RX_BUFFER_ID): cv.declare_id(cg.uint8)})
)


async def to_code(config: ConfigType) -> None:
    # The receive buffer's size is known here, so it is a static array rather than allocated at run time
    rx_buffer = cg.static_buffer(
        config[CONF_RX_BUFFER_ID], config[CONF_MAX_PACKET_SIZE]
    )
    var = await packet_interface.new_packet_interface(config, rx_buffer)
    await uart.register_uart_device(var, config)
