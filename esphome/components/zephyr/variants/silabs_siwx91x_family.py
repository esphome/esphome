"""Silicon Labs SiWx91x Kconfig setup and SPI pinctrl signal decoding, shared
by variants/siwx917.py and zephyr/pinctrl.py.

Deliberately its own family, not silabs_family.py's EFR32 Series 2 family --
see siwx917.py's family= comment for why."""

from esphome.types import ConfigType

# Devicetree property SiWx91x's pinctrl groups pack all their signal macros into.
PROPERTY_NAME = "pinmux"


def pin_macro(prefix: str, signal: str, pin: int | None, variant: str) -> str | None:
    """`prefix` is unused -- unlike EFR32/Nordic/ESP32, SiWx91x has no per-pin
    macro *formula*: only a handful of enumerated candidate pins per signal
    have a macro at all, looked up from the variant's own spi_pin_macros
    (`signal` here is the generic "clk"/"mosi"/"miso" key it's keyed by, not
    a hardware signal name like the other families use)."""
    if pin is None:
        return None
    from . import VARIANTS  # noqa: PLC0415 -- avoids circular import at module load

    return f"<{VARIANTS[variant].spi_pin_macros[signal][pin]}>"


def spi_pinctrl(board: str, bus_label: str) -> None:
    """No board using this family pre-wires this bus's pinctrl today
    (confirmed against siwx917_dk2605a.dts: spi0 ships with no pinctrl-0 at
    all), so there's nothing to content-decode -- group_role_resolver/
    value_role_decoder stay unset, same as a from-scratch board with zero
    existing pinctrl falls back to anyway. Revisit if a future siwx91x board
    ever does pre-wire it."""
    return


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults
    to MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.const import CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import zephyr_add_prj_conf  # noqa: PLC0415 -- avoids circular import at module load

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    # SiWx917 is Cortex-M4F -- has a hardware FPU.
    zephyr_add_prj_conf("FPU", True)
    # random_bytes() uses sys_rand_get(), which requires the entropy subsystem.
    zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when EXTRA_EXCEPTION_INFO
    # is also set (arch/arm/core/Kconfig selects the dependency it needs). SiWx917
    # is Cortex-M4F, same treatment as nordic/silabs/stm32/renesas.
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE":
        zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every silabs_siwx91x-family variant.
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_SILABS_SIWX91X")
