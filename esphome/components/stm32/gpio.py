import re

from esphome import pins
import esphome.codegen as cg
from esphome.components.zephyr.const import zephyr_ns
import esphome.config_validation as cv
from esphome.const import CONF_ANALOG, CONF_ID, CONF_INVERTED, CONF_MODE, CONF_NUMBER

ZephyrGPIOPin = zephyr_ns.class_("ZephyrGPIOPin", cg.InternalGPIOPin)
PIN_RE = re.compile(r"^P([A-P])(1[0-5]|[0-9])$", re.IGNORECASE)


def _translate_pin(value):
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

    parsed = PIN_RE.match(value)
    if parsed:
        port_nr = ord(parsed[1]) - ord("A")
        pin = int(parsed[2])
        return port_nr * 16 + pin

    raise cv.Invalid(f"Invalid pin: {value}")


def validate_gpio_pin(value):
    return _translate_pin(value)


def validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_analog = mode[CONF_ANALOG]
    if is_analog:
        raise cv.Invalid(f"Cannot use {num} as analog pin")
    return value


STM32_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        ZephyrGPIOPin,
        validate_gpio_pin,
        modes=pins.GPIO_STANDARD_MODES + (CONF_ANALOG,),
    ),
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register("stm32", STM32_PIN_SCHEMA)
async def stm32_pin_to_code(config):
    num = config[CONF_NUMBER]
    port = chr(ord("a") + num // 16)
    pin_name_prefix = f"p{port}".upper()
    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio{port}))"),
        16,
        pin_name_prefix,
    )
    cg.add(var.set_pin(num))
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
