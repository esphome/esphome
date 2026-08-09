"""MCUboot swap-method configuration, shared by every ``ota:`` platform that can run on Zephyr.

The MCUboot swap mode is a sysbuild-level choice, not scoped to whichever ota:
platform happens to set it -- platform: esphome and platform: zephyr_mcumgr both end
up pushing the very same MCUBOOT_MODE_SWAP_* sysbuild Kconfig symbol, so the
swap_method: option, its per-variant validation, and the code that applies it all
live here once instead of being duplicated per platform.
"""

import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType

CONF_SWAP_METHOD = "swap_method"

_SYSBUILD_MODE = {
    "scratch": "MCUBOOT_MODE_SWAP_SCRATCH",
    "move": "MCUBOOT_MODE_SWAP_USING_MOVE",
    "offset": "MCUBOOT_MODE_SWAP_USING_OFFSET",
}

SWAP_METHOD_SCHEMA = {
    cv.SplitDefault(
        CONF_SWAP_METHOD,
        zephyr="scratch",
        zephyr_nrf52="offset",
        zephyr_nrf54l15="move",  # pending offset testing, then move default to "offset"
        zephyr_nrf54lm20a="move",  # pending offset testing, then move default to "offset"
        # No scratch partition in this board's flash layout (boot/image-0/
        # image-1/storage only) -- the generic zephyr="scratch" default above
        # isn't valid here, matching nrf52/nrf54l15/nrf54lm20a's own reasoning.
        zephyr_efr32mg24="move",
        zephyr_rp2040="offset",
    ): cv.one_of("scratch", "move", "offset", lower=True),
}


def validate_swap_method(config: ConfigType) -> ConfigType:
    """Reject a swap_method the current variant's port doesn't support."""
    if not CORE.is_zephyr:
        return config
    from . import ZEPHYR_VARIANT_NATIVE_SIM, zephyr_variant
    from .variants import VARIANTS

    variant = zephyr_variant()
    if variant is None or variant == ZEPHYR_VARIANT_NATIVE_SIM:
        return config
    allowed = VARIANTS[variant].swap_methods
    method = config[CONF_SWAP_METHOD]
    if method not in allowed:
        raise cv.Invalid(
            f"'{CONF_SWAP_METHOD}: {method}' is not supported on this variant; "
            f"choose one of {sorted(allowed)}"
        )
    return config


def apply_swap_method(config: ConfigType) -> None:
    """Push the configured MCUboot swap method to sysbuild.conf."""
    if not CORE.is_zephyr:
        return
    from . import ZEPHYR_VARIANT_NATIVE_SIM, zephyr_add_sysbuild_conf, zephyr_variant

    if zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM:
        return
    # A sysbuild-level choice, not per-image prj.conf: the default MCUBOOT_MODE
    # is OVERWRITE_ONLY for the whole family, which silently defeats
    # boot_request_upgrade(BOOT_UPGRADE_TEST) (no image survives to revert to).
    zephyr_add_sysbuild_conf(_SYSBUILD_MODE[config[CONF_SWAP_METHOD]], True)
