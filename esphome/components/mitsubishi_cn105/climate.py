import esphome.codegen as cg
from esphome.components import climate, uart
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_UART_ID

mitsubishi_ns = cg.esphome_ns.namespace("mitsubishi_cn105")

CONF_USE_FAHRENHEIT_CONVERSION = "use_fahrenheit_conversion"

MitsubishiCN105Climate = mitsubishi_ns.class_(
    "MitsubishiCN105Climate",
    climate.Climate,
    cg.Component,
    uart.UARTDevice,
)


def _validate_uart_baud_rate(config):
    uart_config = config[CONF_UART_ID]

    if not isinstance(uart_config, dict):
        raise cv.Invalid("UART config could not be resolved for mitsubishi_cn105")

    baud_rate = uart_config.get(CONF_BAUD_RATE)
    if baud_rate not in (2400, 9600):
        raise cv.Invalid("mitsubishi_cn105 requires UART baud_rate to be 2400 or 9600")

    return config


CONFIG_SCHEMA = (
    climate.climate_schema(MitsubishiCN105Climate)
    .extend(cv.polling_component_schema("1s"))
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_USE_FAHRENHEIT_CONVERSION, default=False): cv.boolean,
        }
    )
)

FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        "mitsubishi_cn105",
        require_rx=True,
        require_tx=True,
        data_bits=8,
        parity="EVEN",
        stop_bits=1,
    ),
    _validate_uart_baud_rate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    cg.add(var.set_use_fahrenheit_conversion(config[CONF_USE_FAHRENHEIT_CONVERSION]))
