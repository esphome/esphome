"""Local controller for Haier hOn water heaters."""

import esphome.codegen as cg
from esphome.components import uart, water_heater
import esphome.config_validation as cv
from esphome.types import ConfigType

CONF_COMMUNICATION_TIMEOUT = "communication_timeout"
CONF_HAIER_WATER_HEATER_ID = "haier_water_heater_id"

CODEOWNERS = ["@TheLexus"]
DEPENDENCIES = ["uart"]

haier_water_heater_ns = cg.esphome_ns.namespace("haier_water_heater")
HonWaterHeater = haier_water_heater_ns.class_(
    "HonWaterHeater", water_heater.WaterHeater, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    water_heater.water_heater_schema(HonWaterHeater)
    .extend(
        {
            cv.Optional(
                CONF_COMMUNICATION_TIMEOUT, default="20s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "haier_water_heater",
    baud_rate=9600,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await water_heater.new_water_heater(config)
    await cg.register_component(var, config)
    cg.add(var.set_communication_timeout(config[CONF_COMMUNICATION_TIMEOUT]))
    await uart.register_uart_device(var, config)
