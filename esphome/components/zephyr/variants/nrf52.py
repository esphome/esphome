import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_OTA,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    KEY_BOOTLOADER,
    KEY_MODULE_REQUESTS,
    ZEPHYR_VARIANT_NRF52,
)
from . import (
    MAINLINE,
    NCS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

_DEFAULT_BOARD = "adafruit_feather_nrf52840"

# advanced: bootloader: -- deliberately local to this variant, not platform: nrf52's own
# BOOTLOADER_* constants (see the sdk= comment below re: issue #11 entanglement).
# BOOTLOADER_MCUBOOT (default): MCUboot is built as a sysbuild child image (see
# _bootloader_to_code).
# BOOTLOADER_ADAFRUIT_NRF52_SD140_V6 skips MCUboot entirely -- the stock board's own
# devicetree (nordic/nrf52840_partition_uf2_sdv6.dtsi, included by
# adafruit_itsybitsy_nrf52840.dts) already reserves the flash regions occupied by a
# factory-installed Adafruit bootloader + SoftDevice v6 as read-only partitions, and
# points zephyr,code-partition at the free gap between them, so the app build lands
# there without overwriting either. See:
# https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather/hathach-memory-map
BOOTLOADER_ADAFRUIT_NRF52_SD140_V6 = "adafruit_nrf52_sd140_v6"
BOOTLOADER_ADAFRUIT_NRF52_SD140_V7 = "adafruit_nrf52_sd140_v7"

# Expected "SoftDevice" partition size for each choice above, cross-checked against
# upstream's own dtsi (nordic/nrf52840_partition_uf2_sdv{6,7}.dtsi): v6 reserves
# 0x26000 (152K), v7 reserves 0x27000 (156K) -- a board whose stock DTS has a
# "SoftDevice" partition of the wrong size for the selected version means the app
# would still be linked against the wrong gap, same footgun as a missing partition.
_SOFTDEVICE_PARTITION_SIZE = {
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: 0x26000,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: 0x27000,
}

_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_MCUBOOT,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
            lower=True,
        ),
    }
)

# GPIO -> nRF52840 SAADC analog-input name. Fixed silicon fact (AIN0-AIN7 datasheet
# pin assignment), independently defined here rather than imported from
# esphome/components/nrf52/const.py's identical AIN_TO_GPIO -- that module belongs to
# the separate NCS-based `platform: nrf52`, and this variant is deliberately
# independent of it (see issue #11: avoiding entanglement between the two was the
# point of building this as a mainline-Zephyr variant in the first place).
# Cross-checked against this exact board's own devicetree: the vbatt divider node
# in adafruit_feather_nrf52840_common.dtsi uses `io-channels = <&adc 5>` (AIN5),
# which is GPIO 29 here -- matching Adafruit's documented P0.29 battery-sense pin.
_ADC_AIN_MAP = {
    2: "AIN0",
    3: "AIN1",
    4: "AIN2",
    5: "AIN3",
    28: "AIN4",
    29: "AIN5",
    30: "AIN6",
    31: "AIN7",
}

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NRF52
VARIANT = ZephyrVariant(
    # NCS (nRF Connect SDK) is the default -- Nordic's own vendor SDK, which is where
    # real hardware support/testing effort for this chip is expected to concentrate.
    # Mainline Zephyr stays available as an alternate (framework: type: zephyr) for
    # anyone who wants to avoid NCS's licensing/tooling footprint, or who hit a
    # regression only present on one side.
    sdk=NCS,
    sdk_name="ncs",
    alt_sdks={"zephyr": MAINLINE},
    family="nordic",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # OpenThread included: issue #11's tracked entanglement was specifically about
    # platform: nrf52's NCS-based OpenThread/Zigbee stack coupling -- picking
    # framework: type: zephyr (mainline) sidesteps that entirely, the same OpenThread
    # source build the esp32-family variants already use. The default
    # framework: type: ncs here uses NCS's own OpenThread, so that original coupling
    # concern applies again there. Zigbee here is a radio-capability declaration only --
    # zigbee_zephyr.py's own gate additionally requires the "zigbee" module (NCS_ZIGBEE_TEMPLATE
    # via NCS.modules) to actually resolve, which only ncs (not mainline) offers.
    transports=frozenset({"openthread", "ble", "zigbee"}),
    soc="nrf52840",
    # No "scratch": neither board defines a scratch_partition (upstream's stock
    # nrf52840 layout never had one either), and move/offset don't need one.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    pwm_node_labels=["pwm0", "pwm1", "pwm2", "pwm3"],
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "nrf52", config, "nRF52840 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        config[CONF_ADVANCED][CONF_BOOTLOADER],
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    bootloader = config[CONF_ADVANCED][CONF_BOOTLOADER]
    if bootloader in (
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    ):
        from ..dts_lookup import get_board_partitions

        # zephyr_to_code() above already fetched this board's DTS (fetch_board_dts) and
        # validated it exists, so this reflects the real board -- not a guess. Only the
        # itsybitsy's stock DTS (nrf52840_partition_uf2_sdv6.dtsi) ships a "SoftDevice"
        # partition; picking this bootloader on a board without one (e.g. the default
        # adafruit_feather_nrf52840, whose nrf52840_partition.dtsi has no such
        # reservation) silently links the app at flash address 0x0 instead of into a
        # real gap, which would overwrite whatever's actually on that board's flash.
        # Warning, not an error: this only sees the board's stock DTS -- a user who
        # added the missing/mismatched partition themselves via `zephyr: overlays:`
        # would be correct and this check can't see that fix.
        partitions = get_board_partitions(config[CONF_BOARD])
        if partitions is not None:
            softdevice = next((p for p in partitions if p[0] == "SoftDevice"), None)
            expected_size = _SOFTDEVICE_PARTITION_SIZE[bootloader]
            if softdevice is None:
                _LOGGER.warning(
                    "Board '%s' has no 'SoftDevice' partition in its stock "
                    "devicetree, so '%s' would not actually protect an existing "
                    "bootloader on this board unless one was added via 'overlays:'. "
                    "Use a board whose stock devicetree ships that partition (e.g. "
                    "adafruit_itsybitsy), or set 'advanced: bootloader: %s' instead.",
                    config[CONF_BOARD],
                    bootloader,
                    BOOTLOADER_MCUBOOT,
                )
            elif softdevice[2] != expected_size:
                _LOGGER.warning(
                    "Board '%s' has a 'SoftDevice' partition of 0x%x bytes, but "
                    "'%s' expects 0x%x bytes -- the app would still be linked "
                    "against the wrong gap unless this was intentionally overridden "
                    "via 'overlays:'.",
                    config[CONF_BOARD],
                    softdevice[2],
                    bootloader,
                    expected_size,
                )

    CORE.add_job(_bootloader_to_code, config)


@coroutine_with_priority(CoroPriority.FINAL)
async def _bootloader_to_code(config: ConfigType) -> None:
    from .. import zephyr_add_sysbuild_conf, zephyr_data

    # ncs-zigbee's default devicetree has no boot/slot1 partitions, so zigbee only
    # gets MCUboot when OTA is actually configured. Deferred to FINAL priority: this
    # variant's own to_code() (above) always runs before zigbee:'s (zigbee depends on
    # zephyr), so checking the module request there would be premature -- by FINAL,
    # zigbee_zephyr.py's request_zephyr_module("zigbee") call has already run if
    # zigbee: is configured.
    # TBD - single slot MCUBOOT
    if (
        "zigbee" not in zephyr_data()[KEY_MODULE_REQUESTS] or CORE.config.get(CONF_OTA)
    ) and zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        # west build always runs with --sysbuild (build_zephyr.py), but sysbuild
        # still needs to be told which bootloader to build as its "mcuboot" child
        # image -- without this, only the (unsigned) app image gets built and
        # flashed.
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
