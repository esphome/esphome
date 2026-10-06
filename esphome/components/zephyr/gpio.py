import re
from typing import TYPE_CHECKING

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
from esphome.core import EsphomeError

from .const import zephyr_ns

if TYPE_CHECKING:
    from .variants import ZephyrVariant

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


def _check_lettered_port(pin: int) -> int:
    """Codegen indexes gpio_port_labels by port, so a flat pin past the last port
    must be rejected here."""
    from . import zephyr_data
    from .variants import VARIANTS

    variant_info = VARIANTS.get(zephyr_data().get("variant"))
    if variant_info is None or variant_info.gpio_port_labels is None:
        return pin
    if pin < 0 or pin // variant_info.gpio_port_width >= len(
        variant_info.gpio_port_labels
    ):
        raise cv.Invalid(
            f"Invalid pin number: {pin} -- this variant has ports "
            f"{variant_info.gpio_port_labels[0].upper()}-"
            f"{variant_info.gpio_port_labels[-1].upper()} with "
            f"{variant_info.gpio_port_width} pins each"
        )
    return pin


def _validate_gpio_pin(value):
    # Accept a flat integer, GPIO<N> notation, or the variant's own vendor pin
    # nomenclature -- for variants with lettered GPIO ports (gpio_port_labels set,
    # e.g. Silicon Labs' PA/PB/PC/PD) that's "PA4"; for port-banked families without
    # letters (gpio.py's own _PORT_BANKED_FAMILIES, e.g. Nordic) that's the "P0.02"
    # style dump_summary() already prints back in logs.
    if isinstance(value, int):
        return _check_lettered_port(value)
    if isinstance(value, str):
        if value.upper().startswith("GPIO"):
            try:
                return _check_lettered_port(int(value[4:]))
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
            pin = int(m.group(2))
            if pin >= variant_info.gpio_port_width:
                raise cv.Invalid(
                    f"'{value}' is not a valid pin: port {letter.upper()} only has "
                    f"pins 0-{variant_info.gpio_port_width - 1}"
                )

            return port_labels.index(letter) * variant_info.gpio_port_width + pin
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
        if value.isdigit():
            return _check_lettered_port(int(value))
    raise cv.Invalid(f"Invalid pin number: {value!r}")


ZEPHYR_PIN_SCHEMA = pins.gpio_base_schema(
    ZephyrGPIOPin,
    _validate_gpio_pin,
    modes=pins.GPIO_STANDARD_MODES,
)


def _pin_name_prefix(
    variant_info: "ZephyrVariant", port: int
) -> tuple[str | None, bool]:
    """Return (prefix, zero_pad_pin) for the variant's own pin notation: "PC" for
    lettered ports, "P0." for port-banked families, "P4" zero-padded for Renesas.
    prefix is None for families with flat GPIO numbering (esp32, rp2040/rp2350)."""
    if (port_labels := variant_info.gpio_port_labels) is not None:
        # Lettered ports (e.g. Silicon Labs' gpioa/gpiob/...) use that vendor's own
        # pin-naming convention directly -- "PA5", not Nordic's "P0.05" style.
        return f"P{port_labels[port].upper()}", False
    if variant_info.family in _PORT_BANKED_FAMILIES:
        return f"P{port}.", False
    if variant_info.family in _CONCAT_PORT_FAMILIES:
        # Renesas RA's own notation always zero-pads the pin to 2 digits (P106, not P16).
        return f"P{port}", True
    return None, False


def pin_summary(variant_info: "ZephyrVariant", num: int) -> str:
    """Return the flat pin plus the variant's own name for it ("GPIO37, PC5"), or just
    "GPIO{num}" where the family has no port notation. Same text as
    ZephyrGPIOPin::dump_summary()."""
    port, pin = divmod(num, variant_info.gpio_port_width)
    if variant_info.gpio_port_labels is not None and port >= len(
        variant_info.gpio_port_labels
    ):
        return f"GPIO{num}"
    prefix, zero_pad = _pin_name_prefix(variant_info, port)
    if prefix is None:
        return f"GPIO{num}"
    return f"GPIO{num}, {prefix}{pin:02d}" if zero_pad else f"GPIO{num}, {prefix}{pin}"


@pins.PIN_SCHEMA_REGISTRY.register(PLATFORM_ZEPHYR, ZEPHYR_PIN_SCHEMA)
async def zephyr_pin_to_code(config):
    from . import zephyr_data
    from .dts_lookup import get_gpio_port_size
    from .variants import VARIANTS

    num = config[CONF_NUMBER]
    variant_info = VARIANTS[zephyr_data()["variant"]]
    gpio_port_width = variant_info.gpio_port_width
    port = num // gpio_port_width
    port_labels = variant_info.gpio_port_labels
    node_suffix = port_labels[port] if port_labels is not None else str(port)
    node_label = f"{variant_info.gpio_node_prefix}{node_suffix}"
    # DEVICE_DT_GET_OR_NULL() would compile a missing pin into a null device.
    port_size = get_gpio_port_size(zephyr_data()["board"], node_label)
    if port_size is not None and num % gpio_port_width >= port_size:
        raise EsphomeError(
            f"Pin {num} does not exist on this board: GPIO controller '{node_label}' "
            + (f"has {port_size} pins" if port_size else "is not in its devicetree")
        )
    args = [
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL({node_label}))"),
        gpio_port_width,
    ]
    prefix, zero_pad = _pin_name_prefix(variant_info, port)
    if prefix is not None:
        args.append(prefix)
        if zero_pad:
            # Tells dump_summary() to zero-pad the pin number to 2 digits.
            args.append(True)
    var = cg.new_Pvariable(*args)
    cg.add(var.set_pin(num))
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
