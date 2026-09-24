"""Nordic (nRF52/nRF54) NRF_PSEL pinctrl signal decoding, shared by I2C/UART/SPI
pinctrl overlay generation in zephyr/__init__.py."""

from collections.abc import Callable

from esphome.types import ConfigType

# Devicetree property nordic pinctrl groups pack all their signal macros into.
PROPERTY_NAME = "psels"


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults
    to MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.const import CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import zephyr_add_prj_conf  # noqa: PLC0415 -- avoids circular import at module load

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    zephyr_add_prj_conf("FPU", True)
    # random_bytes() uses sys_rand_get(), which requires the entropy subsystem.
    zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when EXTRA_EXCEPTION_INFO
    # is also set (arch/arm/core/Kconfig selects the dependency it needs).
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE":
        zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every nordic-family variant (core.cpp, etc.).
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_NORDIC")


# I2C pinctrl group/state to assume when DTS resolution fails -- nordic's I2C
# node pairs a "default" and a "sleep" state, both sharing one group.
I2C_FALLBACK_GROUP = "group1"
I2C_FALLBACK_STATE_SUFFIXES = ("default", "sleep")

# NRF_PSEL's packed value layout (nrf-pinctrl.h): the function-id ("fun") field is
# fixed across every nRF chip generation, unlike ESP32's per-instance signal IDs.
_NRF_PSEL_FUN_SHIFT = 24
_NRF_PSEL_FUN_MASK = 0xFF

# NRF_FUN_TWIM_{SDA,SCL} (nrf-pinctrl.h) -- fixed across every nRF chip.
_NRF_I2C_SIGNAL_NAMES = {12: "sda", 11: "scl"}

# NRF_FUN_UART_{TX,RX,RTS,CTS} (nrf-pinctrl.h) -- fixed across every nRF chip.
_NRF_UART_SIGNAL_NAMES = {0: "tx", 1: "rx", 2: "rts", 3: "cts"}

# NRF_FUN_SPIM_{SCK,MOSI,MISO} (nrf-pinctrl.h) -- fixed across every nRF chip.
_NRF_SPI_SIGNAL_NAMES = {4: "clk", 5: "mosi", 6: "miso"}


def pin_macro(prefix: str, signal: str, pin: int | None, variant: str) -> str | None:
    """`prefix` is the peripheral name (e.g. "TWIM", "UART", "SPIM"). `variant`
    is unused -- accepted only for signature parity with the other families'
    pin_macro()."""
    if pin is None:
        return None
    return f"<NRF_PSEL({prefix}_{signal}, {pin // 32}, {pin % 32})>"


def i2c_value_role(value: int) -> str | None:
    fun = (value >> _NRF_PSEL_FUN_SHIFT) & _NRF_PSEL_FUN_MASK
    return _NRF_I2C_SIGNAL_NAMES.get(fun)


def i2c_value_role_decoder(bus_label: str) -> Callable[[int], str | None] | None:
    """Always available, unlike esp32 which needs bus_label to bind to a
    real instance."""
    return i2c_value_role


def uart_value_role(value: int) -> str | None:
    fun = (value >> _NRF_PSEL_FUN_SHIFT) & _NRF_PSEL_FUN_MASK
    return _NRF_UART_SIGNAL_NAMES.get(fun)


def uart_group_roles(
    board: str, label: str, groups: list[str]
) -> dict[str, str] | None:
    """group_role_resolver reading real psels content instead of assuming
    position -- nrf54lm20dk's uart20 really does swap group order."""
    from ..dts_lookup import get_pinctrl_group_property  # noqa: PLC0415

    signals: dict[str, str] = {}
    for group in groups:
        for value in get_pinctrl_group_property(board, label, group, "psels") or []:
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
    fun = (value >> _NRF_PSEL_FUN_SHIFT) & _NRF_PSEL_FUN_MASK
    return _NRF_SPI_SIGNAL_NAMES.get(fun)


def spi_group_roles(board: str, label: str, groups: list[str]) -> dict[str, str] | None:
    """group_role_resolver reading real psels content -- mirrors uart_group_roles()."""
    from ..dts_lookup import get_pinctrl_group_property  # noqa: PLC0415

    signals: dict[str, str] = {}
    for group in groups:
        for value in get_pinctrl_group_property(board, label, group, "psels") or []:
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
