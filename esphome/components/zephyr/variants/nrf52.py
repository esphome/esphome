import esphome.codegen as cg
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
    CONF_RUNNER,
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

_DEFAULT_BOARD = "adafruit_feather_nrf52840"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA

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
        BOOTLOADER_MCUBOOT,
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
    if "zigbee" not in zephyr_data()[KEY_MODULE_REQUESTS] or CORE.config.get(CONF_OTA):
        # west build always runs with --sysbuild (build_zephyr.py), but sysbuild
        # still needs to be told which bootloader to build as its "mcuboot" child
        # image -- without this, only the (unsigned) app image gets built and
        # flashed.
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)

        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
