import esphome.codegen as cg
from esphome.components import uart
from esphome.components.gnss import GNSSComponent, gnss_schema, new_gnss
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_RATE, CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import ubx_ns

DEPENDENCIES = ["uart"]

CONF_DYNAMIC_MODEL = "dynamic_model"

UBXGNSS = ubx_ns.class_("UBXGNSS", GNSSComponent, uart.UARTDevice)

# CFG-NAV5 dynModel values
DYNAMIC_MODELS = {
    "portable": 0,
    "stationary": 2,
    "pedestrian": 3,
    "automotive": 4,
    "sea": 5,
    "airborne_1g": 6,
    "airborne_2g": 7,
    "airborne_4g": 8,
}

CONFIG_SCHEMA = (
    gnss_schema(UBXGNSS)
    .extend(
        {
            # u-blox 6 measures at up to 5 Hz, later receivers at 10 Hz or more
            cv.Optional(CONF_RATE, default="1Hz"): cv.All(
                cv.frequency, cv.Range(min=0.1, max=25.0)
            ),
            cv.Optional(CONF_DYNAMIC_MODEL): cv.enum(DYNAMIC_MODELS, lower=True),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)
# The longest set of NAV messages per solution (the one before protocol 14) with frame overhead:
# POSLLH 28, DOP 18, SOL 52, VELNED 36 and TIMEUTC 20 payload bytes, 8 bytes per frame
EPOCH_BYTES = 28 + 18 + 52 + 36 + 20 + 5 * 8

# TX sends the configuration
_validate_uart = uart.final_validate_device_schema(
    "ubx", require_rx=True, require_tx=True
)


def _final_validate(config: ConfigType) -> None:
    _validate_uart(config)
    baud_rate = fv.get_declaring_config(config[CONF_UART_ID])[CONF_BAUD_RATE]
    # 10 bits per byte at 8N1
    needed = round(config[CONF_RATE] * EPOCH_BYTES * 10)
    if needed > baud_rate:
        raise cv.Invalid(
            f"A rate of {config[CONF_RATE]:g} Hz needs about {needed} baud for the UBX messages, "
            f"but the uart runs at {baud_rate}; lower the rate or raise the baud rate",
            path=[CONF_RATE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await new_gnss(config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_measurement_rate_ms(round(1000 / config[CONF_RATE])))
    if (model := config.get(CONF_DYNAMIC_MODEL)) is not None:
        cg.add(var.set_dynamic_model(DYNAMIC_MODELS[model]))
