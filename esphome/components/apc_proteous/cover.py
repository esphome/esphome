import esphome.codegen as cg
from esphome.components import cover, uart
import esphome.config_validation as cv

apc_proteous_ns = cg.esphome_ns.namespace("apc_proteous")
APCProteousCover = apc_proteous_ns.class_(
    "APCProteousCover", cover.Cover, cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cover.cover_schema(APCProteousCover)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("500ms"))
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "apc_proteous",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
