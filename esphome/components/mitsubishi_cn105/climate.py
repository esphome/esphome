import esphome.codegen as cg
from esphome.components import climate, uart
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["climate"]
CODEOWNERS = ["@crnjan"]

mitsubishi_ns = cg.esphome_ns.namespace("mitsubishi_cn105")

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    uart.UARTDevice,
)

CONFIG_SCHEMA = (
    climate.climate_schema(MitsubishiCN105Climate)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend({cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.update_interval})
)

FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        "mitsubishi_cn105",
        require_rx=True,
        require_tx=True,
        data_bits=8,
        parity="EVEN",
        stop_bits=1,
    )
)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
