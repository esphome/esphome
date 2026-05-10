import re

from esphome import pins
import esphome.codegen as cg
from esphome.components.zephyr.const import zephyr_ns
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANALOG,
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    PLATFORM_NRF52,
)

from .const import AIN_TO_GPIO, EXTRA_ADC

ZephyrGPIOPin = zephyr_ns.class_("ZephyrGPIOPin", cg.InternalGPIOPin)


def _translate_pin(value):
    if value in AIN_TO_GPIO:
        return AIN_TO_GPIO[value]
    if isinstance(value, dict) or value is None:
        raise cv.Invalid(
            "This variable only supports pin numbers, not full pin schemas "
            "(with inverted and mode)."
        )
    if isinstance(value, int):
        return value
    try:
        return int(value)
    except ValueError:
        pass
    # e.g. P0.27
    match = re.fullmatch(r"P(\d+)\.(\d+)", value)
    if match:
        port = int(match.group(1))
        pin = int(match.group(2))
        return port * 32 + pin
    raise cv.Invalid(f"Invalid pin: {value}")


def validate_gpio_pin(value):
    if value in EXTRA_ADC:
        return value
    value = _translate_pin(value)
    if value < 0 or value > 47:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-47)")
    return value


def validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_analog = mode[CONF_ANALOG]
    if is_analog:
        if num in EXTRA_ADC:
            return value
        if num not in AIN_TO_GPIO.values():
            raise cv.Invalid(f"Cannot use {num} as analog pin")
    return value


NRF52_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        ZephyrGPIOPin,
        validate_gpio_pin,
        modes=pins.GPIO_STANDARD_MODES + (CONF_ANALOG,),
    ),
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register(PLATFORM_NRF52, NRF52_PIN_SCHEMA)
async def nrf52_pin_to_code(config):
    num = config[CONF_NUMBER]
    port = num // 32
    pin_name_prefix = f"P{port}."
    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio{port}))"),
        32,
        pin_name_prefix,
    )
    cg.add(var.set_pin(num))
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
