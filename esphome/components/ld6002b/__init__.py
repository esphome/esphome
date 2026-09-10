from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WAKEUP_PIN
from esphome.types import ConfigType

from .const import CONF_AUTO_WAKE, CONF_WAKEUP_PULSE

CODEOWNERS = ["@hepter"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

ld6002b_ns = cg.esphome_ns.namespace("ld6002b")
LD6002BComponent = ld6002b_ns.class_("LD6002BComponent", cg.Component, uart.UARTDevice)


def _validate_wakeup_options(config: ConfigType) -> ConfigType:
    """Reject wake options that would silently do nothing.

    Runs before the schema so the defaults for the keys below have not been
    filled in yet and an explicit user value is still distinguishable from one.
    """
    if not isinstance(config, dict):
        return config
    if CONF_WAKEUP_PIN in config:
        return config
    for key in (CONF_AUTO_WAKE, CONF_WAKEUP_PULSE):
        if key in config:
            raise cv.Invalid(
                f"'{key}' requires '{CONF_WAKEUP_PIN}' to be configured", path=[key]
            )
    return config


CONFIG_SCHEMA = cv.All(
    _validate_wakeup_options,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD6002BComponent),
            cv.Optional(CONF_WAKEUP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_WAKEUP_PULSE, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_WAKE, default=True): cv.boolean,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld6002b",
    baud_rate=115200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if wakeup_pin_config := config.get(CONF_WAKEUP_PIN):
        pin = await cg.gpio_pin_expression(wakeup_pin_config)
        cg.add(var.set_wakeup_pin(pin))

    cg.add(var.set_wakeup_pulse_ms(config[CONF_WAKEUP_PULSE].total_milliseconds))

    cg.add(var.set_auto_wake(config[CONF_AUTO_WAKE]))
