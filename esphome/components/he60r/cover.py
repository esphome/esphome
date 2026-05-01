import esphome.codegen as cg
from esphome.components import cover, uart
import esphome.config_validation as cv
from esphome.const import CONF_CLOSE_DURATION, CONF_OPEN_DURATION

he60r_ns = cg.esphome_ns.namespace("he60r")
HE60rCover = he60r_ns.class_("HE60rCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = (
    cover.cover_schema(HE60rCover)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            cv.Optional(
                CONF_OPEN_DURATION, default="15s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_CLOSE_DURATION, default="15s"
            ): cv.positive_time_period_milliseconds,
        }
    )
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "he60r",
    baud_rate=1200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
