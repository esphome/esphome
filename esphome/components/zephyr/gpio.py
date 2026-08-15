import re

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

_LETTERED_PIN_RE = re.compile(r"P([A-Za-z])(\d+)")
_DOTTED_PIN_RE = re.compile(r"P(\d+)\.(\d+)")
# Renesas RA's own notation: port digit + 2-digit zero-padded pin, no separator
# (e.g. "P106" = port 1 pin 06).
_CONCAT_PIN_RE = re.compile(r"P(\d)(\d{2})")
_CONCAT_PORT_FAMILIES = {"renesas"}


def _validate_gpio_pin(value):
    # Accept a flat integer, GPIO<N> notation, or the variant's own vendor pin
    # nomenclature -- for variants with lettered GPIO ports (gpio_port_labels set,
    # e.g. Silicon Labs' PA/PB/PC/PD) that's "PA4"; for port-banked families without
    # letters (gpio.py's own _PORT_BANKED_FAMILIES, e.g. Nordic) that's the "P0.02"
    # style dump_summary() already prints back in logs.
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        if value.upper().startswith("GPIO"):
            try:
                return int(value[4:])
            except ValueError as exc:
                raise cv.Invalid(f"Invalid pin: {value}") from exc
        if (m := _LETTERED_PIN_RE.fullmatch(value)) is not None:
            from . import zephyr_data
            from .variants import VARIANTS

            variant_info = VARIANTS.get(zephyr_data().get("variant"))
            port_labels = (
                variant_info.gpio_port_labels if variant_info is not None else None
            )
            if port_labels is None:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: this variant uses flat pin "
                    f"numbers (e.g. '{m.group(2)}'), not lettered-port notation."
                )
            letter = m.group(1).lower()
            if letter not in port_labels:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: unknown port '{letter.upper()}'"
                )
            return port_labels.index(letter) * variant_info.gpio_port_width + int(
                m.group(2)
            )
        if (m := _DOTTED_PIN_RE.fullmatch(value)) is not None:
            from . import zephyr_data
            from .variants import VARIANTS

            variant_info = VARIANTS.get(zephyr_data().get("variant"))
            if variant_info is None or variant_info.family not in _PORT_BANKED_FAMILIES:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: this variant does not use "
                    f"'P<port>.<pin>' notation."
                )
            port, pin = int(m.group(1)), int(m.group(2))
            if pin >= variant_info.gpio_port_width:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: port {port} only has pins "
                    f"0-{variant_info.gpio_port_width - 1}."
                )
            return port * variant_info.gpio_port_width + pin
        if (m := _CONCAT_PIN_RE.fullmatch(value)) is not None:
            from . import zephyr_data
            from .variants import VARIANTS

            variant_info = VARIANTS.get(zephyr_data().get("variant"))
            if variant_info is None or variant_info.family not in _CONCAT_PORT_FAMILIES:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: this variant does not use "
                    f"'P<port><pin>' notation."
                )
            port, pin = int(m.group(1)), int(m.group(2))
            if pin >= variant_info.gpio_port_width:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: port {port} only has pins "
                    f"0-{variant_info.gpio_port_width - 1}."
                )
            return port * variant_info.gpio_port_width + pin
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
    port_labels = variant_info.gpio_port_labels
    node_suffix = port_labels[port] if port_labels is not None else str(port)
    args = [
        config[CONF_ID],
        cg.RawExpression(
            f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL({variant_info.gpio_node_prefix}{node_suffix}))"
        ),
        gpio_port_width,
    ]
    if port_labels is not None:
        # Lettered ports (e.g. Silicon Labs' gpioa/gpiob/...) use that vendor's own
        # pin-naming convention directly -- "PA5", not Nordic's "P0.05" style.
        args.append(f"P{node_suffix.upper()}")
    elif variant_info.family in _PORT_BANKED_FAMILIES:
        args.append(f"P{port}.")
    elif variant_info.family in _CONCAT_PORT_FAMILIES:
        # Renesas RA's own notation always zero-pads the pin to 2 digits (P106, not
        # P16) -- the trailing `True` tells dump_summary() to format accordingly.
        args.append(f"P{port}")
        args.append(True)
    var = cg.new_Pvariable(*args)
    cg.add(var.set_pin(num))
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
