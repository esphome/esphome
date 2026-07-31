import esphome.codegen as cg
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    KEY_FRAMEWORK_VERSION,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_NRF54L15
from . import (
    MAINLINE,
    NCS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_DEFAULT_BOARD = "nrf54l15dk"
# qualify_board() expands this to "nrf54l15dk/nrf54l15/cpuapp" via soc=/qualifier= below --
# the application core target confirmed in upstream Zephyr's own board.yml for this DK
# (boards/nordic/nrf54l15dk/board.yml lists nrf54l15's cpuapp/cpuflpr cpuclusters). Only
# the application core is supported here -- cpuflpr (the RISC-V co-processor core) is a
# separate, much more specialized target this variant does not build for.
_VALID_BOARDS = [_DEFAULT_BOARD]

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NRF54L15
VARIANT = ZephyrVariant(
    # Same reasoning as nrf52: NCS is Nordic's own SDK and where support for its own
    # newest silicon lands and gets tested first. Mainline Zephyr (which already carries
    # this board's definition too) stays available as an alternate.
    sdk=NCS,
    sdk_name="ncs",
    alt_sdks={"zephyr": MAINLINE},
    family="nordic",
    boards=_VALID_BOARDS,
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # Zigbee excluded: ZBOSS (Nordic's Zigbee stack) is only wired up for the separate,
    # NCS-coupled `platform: nrf52` -- same boundary nrf52's own zephyr variant draws.
    transports=frozenset({"ble", "openthread"}),
    soc="nrf54l15",
    qualifier="cpuapp",
    # Conservative default: only the universally-supported MCUboot swap mode is claimed
    # here. Unlike nrf52840 (single flash bank, well-understood offset/move behavior),
    # this variant hasn't been validated against the other swap modes yet.
    swap_methods=frozenset({"move"}),
    # nrf54l15dk_common.dtsi only enables uart20 (routed to the DK's VCOM0/J-Link USB
    # serial, the natural default) and uart30 -- there is no uart0/uart1 node on this SoC.
    uart_node_labels={"UART0": "uart20", "UART1": "uart30"},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "nrf54l15", config, "nRF54L15 support"
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
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NRF54L15")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF54L15")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    # Neither the SoC devicetree nor this board enables a watchdog or aliases
    # it as `watchdog0`, which esphome/components/zephyr/hal.cpp requires
    # unconditionally. WDT30 is reserved for the Secure domain on this SoC
    # (see nrf54l_05_10_15.dtsi); WDT31 is the one available to the
    # application core we build for here.
    zephyr_add_overlay(
        """
        / {
            aliases {
                watchdog0 = &wdt31;
            };
        };

        &wdt31 {
            status = "okay";
        };
        """
    )

    # RSA-2048 (mcuboot's default) is code-size heavy; ECDSA-P256 has a much
    # smaller footprint. Same tradeoff nrf52 makes -- strictly smaller either way.
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")
