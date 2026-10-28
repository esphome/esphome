"""MCUboot swap-method configuration, shared by every ``ota:`` platform that can run on Zephyr.

The MCUboot swap mode is a sysbuild-level choice, not scoped to whichever ota:
platform happens to set it -- platform: esphome and platform: zephyr_mcumgr both end
up pushing the very same MCUBOOT_MODE_SWAP_* sysbuild Kconfig symbol, so the
per-variant validation and the code that applies it live here once instead of
being duplicated per platform. The swap_method: schema itself lives in
esphome.components.ota instead (see CONF_SWAP_METHOD/SWAP_METHOD_SCHEMA there):
it has no zephyr dependency, and platform: esphome needs it at module-load
time on every platform, not just Zephyr.
"""

from esphome.components.ota import CONF_SWAP_METHOD, reject_swap_method_off_zephyr
import esphome.config_validation as cv
from esphome.const import CONF_OTA
from esphome.core import CORE
from esphome.types import ConfigType

_SYSBUILD_MODE = {
    "scratch": "MCUBOOT_MODE_SWAP_SCRATCH",
    "move": "MCUBOOT_MODE_SWAP_USING_MOVE",
    "offset": "MCUBOOT_MODE_SWAP_USING_OFFSET",
}


def validate_swap_method(config: ConfigType) -> ConfigType:
    """Reject a swap_method the current variant's port doesn't support."""
    if not CORE.is_zephyr:
        return reject_swap_method_off_zephyr(config)
    from . import zephyr_variant
    from .variants import VARIANTS

    variant = zephyr_variant()
    if variant is None:
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
    from . import zephyr_add_sysbuild_conf

    method = config[CONF_SWAP_METHOD]
    # A sysbuild-level choice, not per-image prj.conf: the default MCUBOOT_MODE
    # is OVERWRITE_ONLY for the whole family, which silently defeats
    # boot_request_upgrade(BOOT_UPGRADE_TEST) (no image survives to revert to).
    zephyr_add_sysbuild_conf(_SYSBUILD_MODE[method], True)


def zephyr_skip_bootloader_supported() -> bool:
    """True when MCUboot is in the build: only with ota:, and OTA never sends it."""
    return CORE.is_zephyr and bool((CORE.config or {}).get(CONF_OTA))


def apply_single_slot() -> None:
    """Push zephyr: single_slot: true to sysbuild.conf.

    Called directly from zephyr_to_code(), independent of whether any ota:
    platform is configured -- unlike apply_swap_method(), single_slot: has no
    ota: dependency at all (see ota/__init__.py's _ota_final_validate, which
    rejects any ota: platform when single_slot: true is set).
    """
    from . import zephyr_add_sysbuild_conf

    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_add_sysbuild_conf("MCUBOOT_MODE_SINGLE_APP", True)
