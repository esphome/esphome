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
    if len(value) >= len("P0.0") and value[0] == "P" and value[2] == ".":
        return cv.int_(value[len("P")].strip()) * 32 + cv.int_(
            value[len("P0.") :].strip()
        )
    raise cv.Invalid(f"Invalid pin: {value}")


def validate_gpio_pin(value):
    if value in EXTRA_ADC:
        return value
    value = _translate_pin(value)
    if value < 0 or value > (32 + 16):
        raise cv.Invalid(f"NRF52: Invalid pin number: {value}")
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
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
