"""Hi-Link LD6004 TinyFrame radar."""

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CONF_STALE_TIMEOUT: str = "stale_timeout"
CONF_OUT_PIN: str = "out_pin"

CODEOWNERS: list[str] = ["@wolph"]
DEPENDENCIES: list[str] = ["uart"]
MULTI_CONF: bool = True
ld6004_ns: cg.MockObj = cg.esphome_ns.namespace("ld6004")
LD6004Component: cg.MockObjClass = ld6004_ns.class_(
    "LD6004Component", cg.Component, uart.UARTDevice
)
CONF_LD6004_ID: str = "ld6004_id"
CONFIG_SCHEMA: cv.Schema = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD6004Component),
            cv.Optional(
                CONF_STALE_TIMEOUT, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OUT_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA: cv.Schema = uart.final_validate_device_schema(
    "ld6004",
    baud_rate=115200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var: cg.MockObj = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_stale_timeout(config[CONF_STALE_TIMEOUT].total_milliseconds))
    pin_config: ConfigType | None
    if pin_config := config.get(CONF_OUT_PIN):
        pin: cg.MockObj = await cg.gpio_pin_expression(pin_config)
        cg.add(var.set_out_pin(pin))
