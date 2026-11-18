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
    # The receive buffer's size is known here, so it is a static array rather than allocated at run time
    rx_buffer = f"{config[CONF_ID].id}__rx_buffer"
    cg.add_global(
        cg.RawStatement(f"static uint8_t {rx_buffer}[{config[CONF_MAX_PACKET_SIZE]}];")
    )
    var = await packet_interface.new_packet_interface(
        config, cg.RawExpression(rx_buffer)
    )
    await uart.register_uart_device(var, config)
