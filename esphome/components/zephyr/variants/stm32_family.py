"""STM32-family zephyr_to_code hook.

No pinctrl-specific code yet -- STM32's I2C/UART/SPI pins are a small,
per-signal set of pre-named alternate-function phandles Zephyr already
declares (not free-mux like esp32/nordic/silabs), and remapping them isn't
implemented yet. See platform_zephyr_todo memory for the open item."""

from esphome.types import ConfigType


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults
    to MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.const import CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import (  # noqa: PLC0415 -- avoids circular import at module load
        zephyr_add_prj_conf,
        zephyr_variant,
    )
    from ..const import ZEPHYR_VARIANT_STM32F1, ZEPHYR_VARIANT_STM32F4  # noqa: PLC0415

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    zephyr_add_prj_conf("FPU", True)
    # random_bytes() uses sys_rand_get(), which requires the entropy subsystem.
    # STM32F4 is a whole chip family, not a single SoC -- RNG presence varies per
    # member (F401/F411 have none, F405/F410/F412 and larger do), so stm32f4.py
    # resolves this itself from the board's own DTS instead of a blanket default.
    # STM32F1 has no true RNG on any family member (dts/arm/st/f1 has no rng@ node
    # at all), so stm32f1.py always uses TEST_RANDOM_GENERATOR unconditionally.
    if zephyr_variant() not in (ZEPHYR_VARIANT_STM32F1, ZEPHYR_VARIANT_STM32F4):
        zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when EXTRA_EXCEPTION_INFO
    # is also set (arch/arm/core/Kconfig selects the dependency it needs).
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE":
        zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every stm32-family variant (core.cpp, etc.).
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_STM32")
