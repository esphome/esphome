from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    PLATFORM_ZEPHYR,
)

from .const import zephyr_ns

ZephyrGPIOPin = zephyr_ns.class_("ZephyrGPIOPin", cg.InternalGPIOPin)

# Explicit allowlist, not a default-on fallback: future non-Nordic families (e.g.
# STM32's GPIOA/GPIOB port letters) may use a different port-bank scheme entirely.
_PORT_BANKED_FAMILIES = {"nordic"}


def _validate_gpio_pin(value):
    # Accept a flat integer or GPIO<N> notation.
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        if value.upper().startswith("GPIO"):
            try:
                return int(value[4:])
            except ValueError as exc:
                raise cv.Invalid(f"Invalid pin: {value}") from exc
        try:
            return int(value)
        except ValueError:
            pass
    raise cv.Invalid(f"Invalid pin number: {value!r}")


ZEPHYR_PIN_SCHEMA = pins.gpio_base_schema(
    ZephyrGPIOPin,
    _validate_gpio_pin,
    modes=pins.GPIO_STANDARD_MODES,
)


@pins.PIN_SCHEMA_REGISTRY.register(PLATFORM_ZEPHYR, ZEPHYR_PIN_SCHEMA)
async def zephyr_pin_to_code(config):
    from . import zephyr_data
    from .variants import VARIANTS

    num = config[CONF_NUMBER]
    variant_info = VARIANTS[zephyr_data()["variant"]]
    gpio_port_width = variant_info.gpio_port_width
    port = num // gpio_port_width
    args = [
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio{port}))"),
        gpio_port_width,
    ]
    if variant_info.family in _PORT_BANKED_FAMILIES:
        args.append(f"P{port}.")
    var = cg.new_Pvariable(*args)
    cg.add(var.set_pin(num))
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
