from esphome.components import packet_interface, uart
from esphome.components.packet_interface import packet_interface_schema
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
    var = await packet_interface.new_packet_interface(config)
    await uart.register_uart_device(var, config)
