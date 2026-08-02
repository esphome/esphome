import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    KEY_FRAMEWORK_VERSION,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_NRF54LM20A
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_DEFAULT_BOARD = "xiao_nrf54lm20a"
# qualify_board() expands this to "xiao_nrf54lm20a/nrf54lm20a/cpuapp" via soc=/qualifier=
# below -- the application core target confirmed in upstream Zephyr's own board.yml for
# this board. Only the application core is supported here -- cpuflpr (the RISC-V
# co-processor core) is a separate, much more specialized target this variant does not
# build for.

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NRF54LM20A
VARIANT = ZephyrVariant(
    # Unlike nrf52/nrf54l15, NCS is *not* the default here: this board (merged upstream
    # 2026-05-22) isn't in nrfconnect/sdk-zephyr yet, even at that fork's own HEAD --
    # mainline Zephyr is the only SDK that actually carries it right now, so it's the
    # only one registered. Revisit once NCS syncs it in.
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="nordic",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # Zigbee excluded: ZBOSS (Nordic's Zigbee stack) is only wired up for the separate,
    # NCS-coupled `platform: nrf52` -- same boundary nrf54l15's zephyr variant draws.
    transports=frozenset({"ble", "openthread"}),
    soc="nrf54lm20a",
    qualifier="cpuapp",
    # Conservative default: only the universally-supported MCUboot swap mode is claimed
    # here, same reasoning as nrf54l15 -- this variant hasn't been validated against the
    # other swap modes yet.
    swap_methods=frozenset({"move"}),
    # xiao_nrf54lm20a_nrf54lm20a-common.dtsi enables uart20 (console, routed to the
    # onboard CMSIS-DAP's USB-serial bridge) and uart21 (the XIAO header's TX/RX pins) --
    # there is no uart0/uart1 node on this SoC.
    uart_node_labels={"UART0": "uart20", "UART1": "uart21"},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    # No tagged Zephyr release (up to and including the latest, v4.4.1) contains this
    # board yet -- framework: version: would silently resolve to a tag that can't build
    # it, failing confusingly deep inside `west build`. Fail early instead.
    if CONF_SOURCE not in config[CONF_FRAMEWORK]:
        raise cv.Invalid(
            "The nRF54LM20A variant requires 'framework: source:' -- this board isn't "
            "in any tagged Zephyr release yet. Point it at mainline Zephyr's main "
            "branch: framework: source: type: git, ref: main",
            [CONF_FRAMEWORK],
        )
    version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "nrf54lm20a", config, "nRF54LM20A support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        BOOTLOADER_MCUBOOT,
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
    )
    config[KEY_FRAMEWORK_VERSION] = version_str
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_overlay,
        zephyr_add_prj_conf,
        zephyr_setup_preferences,
        zephyr_to_code,
    )

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NRF54LM20A")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF54LM20A")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    # Unlike nrf54l15, this board's own devicetree already aliases `watchdog0` to
    # &wdt31 (xiao_nrf54lm20a_nrf54lm20a-common.dtsi) -- it just leaves the peripheral
    # itself disabled by default, same as the bare SoC dtsi. WDT30 is reserved for the
    # Secure domain on this SoC; WDT31 is the one available to the application core we
    # build for here, so only the `status` needs setting.
    zephyr_add_overlay(
        """
        &wdt31 {
            status = "okay";
        };
        """
    )

    # RSA-2048 (mcuboot's default) is code-size heavy; ECDSA-P256 has a much
    # smaller footprint. Same tradeoff nrf52/nrf54l15 make -- strictly smaller either way.
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")

    # This board's devicetree marks its I2C buses, PMIC (nPM1300, via MFD), and fixed
    # regulator "okay" unconditionally (not something app config controls), which
    # Kconfig's `default y if DT_HAS_..._ENABLED` pattern auto-selects into every
    # image built against this board -- including mcuboot's own minimal one. Those
    # drivers call k_mutex_*/k_work_submit, which don't exist without
    # CONFIG_MULTITHREADING -- and mcuboot disables that for a smaller footprint.
    # mcuboot itself needs none of these (it only touches flash/watchdog), so turn
    # them back off for that image specifically rather than dragging mcuboot's
    # threading model in just to satisfy drivers it never calls into.
    zephyr_add_prj_conf("I2C", False, image="mcuboot")
    zephyr_add_prj_conf("REGULATOR", False, image="mcuboot")
    zephyr_add_prj_conf("MFD", False, image="mcuboot")
