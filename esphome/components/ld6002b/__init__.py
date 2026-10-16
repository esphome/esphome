from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM, CONF_WAKEUP_PIN
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import CONF_AUTO_WAKE, CONF_LD6002B_ID, CONF_WAKEUP_PULSE

CODEOWNERS = ["@hepter"]
DEPENDENCIES = ["uart"]
DOMAIN = "ld6002b"
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


def validate_unique_settings(domain: str, config: ConfigType) -> None:
    """Reject a setting configured in two platform blocks of the same hub; the hub keeps one entity per setting."""
    hub_id = config[CONF_LD6002B_ID]
    entries = [
        entry
        for entry in fv.full_config.get().get(domain, [])
        if entry.get(CONF_PLATFORM) == DOMAIN and entry.get(CONF_LD6002B_ID) == hub_id
    ]
    for key in config:
        if key in (CONF_PLATFORM, CONF_LD6002B_ID):
            continue
        if sum(key in entry for entry in entries) > 1:
            raise cv.Invalid(
                f"{key} is already set for this ld6002b in another {domain} block",
                path=[key],
            )
