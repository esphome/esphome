from esphome import pins

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_INVERTED,
    CONF_NUMBER,
)

nrf52_ns = cg.esphome_ns.namespace("nrf52")
NRF52GPIOPin = nrf52_ns.class_("NRF52GPIOPin", cg.InternalGPIOPin)


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
    # e.g. P0.27
    if len(value) >= len("P0.0") and value[0] == "P" and value[2] == ".":
        return cv.int_(value[len("P")].strip()) * 32 + cv.int_(
            value[len("P0.") :].strip()
        )
    raise cv.Invalid(f"Invalid pin: {value}")


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if value < 0 or value > (32 + 16):
        raise cv.Invalid(f"NRF52: Invalid pin number: {value}")
    return value


NRF52_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        NRF52GPIOPin,
        validate_gpio_pin,
    ),
)


@pins.PIN_SCHEMA_REGISTRY.register("nrf52", NRF52_PIN_SCHEMA)
async def nrf52_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
