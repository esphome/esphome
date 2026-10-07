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

import esphome.codegen as cg
from esphome.components.ota import CONF_SWAP_METHOD, reject_swap_method_off_zephyr
import esphome.config_validation as cv
from esphome.const import CONF_OTA
from esphome.core import CORE
from esphome.types import ConfigType

_SYSBUILD_MODE = {
    "scratch": "MCUBOOT_MODE_SWAP_SCRATCH",
    "move": "MCUBOOT_MODE_SWAP_USING_MOVE",
    "offset": "MCUBOOT_MODE_SWAP_USING_OFFSET",
    # _WITH_REVERT, not plain DIRECT_XIP: without it there's no confirm/revert state at
    # all, so a bad image would boot-loop forever instead of falling back like the
    # other three modes do via BOOT_UPGRADE_TEST.
    "direct": "MCUBOOT_MODE_DIRECT_XIP_WITH_REVERT",
}


def validate_swap_method(config: ConfigType) -> ConfigType:
    """Reject a swap_method the current variant's port doesn't support."""
    if not CORE.is_zephyr:
        return reject_swap_method_off_zephyr(config)
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
    from . import (
        ZEPHYR_VARIANT_NATIVE_SIM,
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_variant,
    )

    if zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM:
        return
    method = config[CONF_SWAP_METHOD]
    # A sysbuild-level choice, not per-image prj.conf: the default MCUBOOT_MODE
    # is OVERWRITE_ONLY for the whole family, which silently defeats
    # boot_request_upgrade(BOOT_UPGRADE_TEST) (no image survives to revert to).
    zephyr_add_sysbuild_conf(_SYSBUILD_MODE[method], True)
    if method == "direct":
        cg.add_define("USE_OTA_ZEPHYR_DIRECT_XIP")
        # Bootloader-image Kconfig, no SB_CONFIG_* mirror exists for it. Without it
        # MCUboot's version compare ignores the build number, so two slots signed at
        # the same major.minor.revision (the default, absent an app VERSION file)
        # would tie forever.
        zephyr_add_prj_conf("BOOT_VERSION_CMP_USE_BUILD_NUMBER", True, image="mcuboot")
        # Constant: a changing value rewrites autoconf.h and rebuilds everything.
        # build_zephyr.resign_direct_xip_images() applies the per-build number.
        zephyr_add_prj_conf("MCUBOOT_IMGTOOL_SIGN_VERSION", "0.0.0+0")


def zephyr_swap_method() -> str | None:
    """Return the configured MCUboot swap method, or None if not applicable.

    Read from the config, not to_code() state: standalone upload (which may load the
    cached config and skip validation) needs it to pick the direct-xip slot image.
    """
    from . import ZEPHYR_VARIANT_NATIVE_SIM, zephyr_variant  # noqa: PLC0415

    if not CORE.is_zephyr or zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM:
        return None
    # ota.final_validate guarantees every entry agrees.
    for ota_conf in (CORE.config or {}).get(CONF_OTA, []):
        if (method := ota_conf.get(CONF_SWAP_METHOD)) is not None:
            return method
    return None


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
    cg.add_define("USE_ZEPHYR_MCUBOOT_SINGLE_SLOT")
