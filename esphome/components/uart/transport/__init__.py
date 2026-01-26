import esphome.codegen as cg
from esphome.components import transport, uart
from esphome.components.transport import transport_schema
import esphome.config_validation as cv
from esphome.const import CONF_RX_BUFFER_SIZE

from .. import uart_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["uart"]

UARTTransport = uart_ns.class_(
    "UartTransport", transport.Transport, uart.UARTDevice, cg.PollingComponent
)

CONFIG_SCHEMA = (
    transport_schema(UARTTransport)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_RX_BUFFER_SIZE, default=1024): cv.int_range(
                256, 8192, min_included=True, max_included=True
            ),
        }
    )
)


async def to_code(config):
    var = await transport.new_transport(config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
