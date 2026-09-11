"""Silicon Labs (EFR32MG24) SILABS_DBUS pinctrl signal decoding, shared by
I2C/UART/SPI pinctrl overlay generation in zephyr/__init__.py."""

from collections.abc import Callable

# Devicetree property silabs pinctrl groups pack all their signal macros into.
PROPERTY_NAME = "pins"

# I2C pinctrl group/state to assume when DTS resolution fails.
I2C_FALLBACK_GROUP = "group0"
I2C_FALLBACK_STATE_SUFFIXES = ("default",)

# SILABS_DBUS's packed value layout (silabs-pinctrl-dbus.h): the en_bit field
# identifies which peripheral signal a pin is routed to, fixed per signal across
# every USART/EUSART instance/chip (verified xg22/xg24/xg26).
_SILABS_DBUS_EN_BIT_SHIFT = 19
_SILABS_DBUS_EN_BIT_MASK = 0x1F

# I2C0/I2C1 en_bit -- distinct from UART's/SPI's en_bits.
_SILABS_I2C_SIGNAL_NAMES = {0: "scl", 1: "sda"}

# USART/EUSART en_bit: TX=4, RX=2, RTS=1, CTS=0.
_SILABS_UART_SIGNAL_NAMES = {4: "tx", 2: "rx", 1: "rts", 0: "cts"}

# USART SPI-mode en_bit -- same TX/RX macros UART uses, repurposed.
_SILABS_SPI_SIGNAL_NAMES = {4: "mosi", 2: "miso", 3: "clk"}


def pin_macro(prefix: str, signal: str, pin: int | None, variant: str) -> str | None:
    if pin is None:
        return None
    from . import VARIANTS  # noqa: PLC0415 -- avoids circular import at module load

    port_width = VARIANTS[variant].gpio_port_width
    letter, num = chr(ord("A") + pin // port_width), pin % port_width
    return f"<{prefix}_{signal}_P{letter}{num}>"


def i2c_value_role(value: int) -> str | None:
    en_bit = (value >> _SILABS_DBUS_EN_BIT_SHIFT) & _SILABS_DBUS_EN_BIT_MASK
    return _SILABS_I2C_SIGNAL_NAMES.get(en_bit)


def i2c_value_role_decoder(bus_label: str) -> Callable[[int], str | None] | None:
    """Always available, unlike esp32 which needs bus_label to bind to a
    real instance."""
    return i2c_value_role


def uart_value_role(value: int) -> str | None:
    en_bit = (value >> _SILABS_DBUS_EN_BIT_SHIFT) & _SILABS_DBUS_EN_BIT_MASK
    return _SILABS_UART_SIGNAL_NAMES.get(en_bit)


def uart_group_roles(
    board: str, label: str, groups: list[str]
) -> dict[str, str] | None:
    """group_role_resolver reading real `pins` content instead of assuming
    position -- xg24_ek2703a's usart0 splits TX(group0)/RX(group1)."""
    from ..dts_lookup import get_pinctrl_group_property  # noqa: PLC0415

    signals: dict[str, str] = {}
    for group in groups:
        for value in get_pinctrl_group_property(board, label, group, "pins") or []:
            name = uart_value_role(value)
            if name is not None:
                signals[name] = group
    if "tx" not in signals or "rx" not in signals:
        return None
    return signals


def uart_pinctrl(
    board: str, port_label: str
) -> (
    tuple[
        Callable[[str, str, list[str]], dict[str, str] | None],
        Callable[[int], str | None],
    ]
    | None
):
    """Always available, unlike esp32/rpi_pico which need port_label to bind
    to a real instance."""
    return uart_group_roles, uart_value_role


def spi_value_role(value: int) -> str | None:
    en_bit = (value >> _SILABS_DBUS_EN_BIT_SHIFT) & _SILABS_DBUS_EN_BIT_MASK
    return _SILABS_SPI_SIGNAL_NAMES.get(en_bit)


def spi_group_roles(board: str, label: str, groups: list[str]) -> dict[str, str] | None:
    """group_role_resolver reading real pins content -- mirrors uart_group_roles()."""
    from ..dts_lookup import get_pinctrl_group_property  # noqa: PLC0415

    signals: dict[str, str] = {}
    for group in groups:
        for value in get_pinctrl_group_property(board, label, group, "pins") or []:
            name = spi_value_role(value)
            if name is not None:
                signals[name] = group
    return signals or None


def spi_pinctrl(
    board: str, bus_label: str
) -> (
    tuple[
        Callable[[str, str, list[str]], dict[str, str] | None],
        Callable[[int], str | None],
    ]
    | None
):
    """Always available, unlike esp32 which needs bus_label to bind to a
    real instance."""
    return spi_group_roles, spi_value_role
