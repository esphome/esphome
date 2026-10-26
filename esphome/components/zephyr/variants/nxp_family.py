"""NXP-family zephyr_to_code hook.

No pinctrl-specific code yet -- remapping NXP pins isn't implemented."""

from esphome.types import ConfigType

from ..const import SEGGER_RTT_UART
from . import mcuboot_or_none

# Software UART drivers (devicetree compatible strings) that work on this family,
# on top of its hardware UARTs.
UART_DRIVERS: frozenset[str] = frozenset({SEGGER_RTT_UART})

# Vendor HAL west projects this family builds against on mainline Zephyr; the other
# vendor HALs are not fetched.
WEST_HALS: frozenset[str] = frozenset({"hal_nxp"})


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults
    to MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.const import CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import zephyr_add_prj_conf  # noqa: PLC0415 -- avoids circular import at module load

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    zephyr_add_prj_conf("FPU", True)
    # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when EXTRA_EXCEPTION_INFO
    # is also set (arch/arm/core/Kconfig selects the dependency it needs).
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE":
        zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every nxp-family variant (api_connection.cpp, etc.).
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_NXP")


def bootloader(advanced: ConfigType) -> str:
    """Return the bootloader set_core_data() gets, rebuilt from the cached advanced: block."""
    return mcuboot_or_none(advanced)
