import esphome.codegen as cg
from esphome.components import packet_interface, uart
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    packet_interface_schema,
)
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import uart_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["uart"]

UartPacketInterface = uart_ns.class_(
    "UartPacketInterface",
    packet_interface.PacketInterface,
    uart.UARTDevice,
)

CONFIG_SCHEMA = packet_interface_schema(UartPacketInterface, 508).extend(
    uart.UART_DEVICE_SCHEMA
)


async def to_code(config: ConfigType) -> None:
    rx_buffer = cg.static_buffer(
        config[CONF_ID], cg.uint8, config[CONF_MAX_PACKET_SIZE]
    )
    var = await packet_interface.new_packet_interface(config, rx_buffer)
    await uart.register_uart_device(var, config)
