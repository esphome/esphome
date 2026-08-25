import esphome.codegen as cg
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    CONF_RUNNER,
    ZEPHYR_VARIANT_NRF54L15,
)
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

_ADVANCED_SCHEMA = ADVANCED_SCHEMA

# GPIO -> nRF54L15 SAADC analog-input name. Fixed silicon fact (AIN0-AIN7 datasheet pin
# assignment) -- not discoverable from any board's DTS, same reasoning as nrf52's own
# _ADC_AIN_MAP. Cross-checked against Nordic's nRF54L15/nRF54L10/nRF54L05 datasheet
# v1.0 (QFN pinout tables): P1.04/AIN0, P1.05/AIN1, P1.06/AIN2, P1.07/AIN3, P1.11/AIN4,
# P1.12/AIN5, P1.13/AIN6, P1.14/AIN7. Note this SoC has 14 SAADC channels total
# (AIN0-AIN13 per nrf-saadc.h), but only AIN0-AIN7 are broken out to GPIO pins --
# AIN8-AIN13 are internal/reserved, so this map only needs the first 8.
# Unlike nrf52 (single GPIO port, so pin number == AIN's flat GPIO number directly),
# these pins are all on P1 -- flat number is port * gpio_port_width(32) + pin, e.g.
# P1.04 -> 36.
_ADC_AIN_MAP = {
    36: "AIN0",  # P1.04
    37: "AIN1",  # P1.05
    38: "AIN2",  # P1.06
    39: "AIN3",  # P1.07
    43: "AIN4",  # P1.11
    44: "AIN5",  # P1.12
    45: "AIN6",  # P1.13
    46: "AIN7",  # P1.14
}

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
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # Untested on hardware as of 2026-08-04.
    transports=frozenset({"ble", "openthread", "zigbee"}),
    soc="nrf54l15",
    qualifier="cpuapp",
    # No "scratch": neither board defines a scratch_partition (upstream's stock
    # nrf52840 layout never had one either), and move/offset don't need one.
    # offset is untested on hardware as of 20260802, update this comment when tested
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    # nrf54l15dk_common.dtsi only enables uart20 (routed to the DK's VCOM0/J-Link USB
    # serial, the natural default) and uart30 -- there is no uart0/uart1 node on this SoC.
    uart_node_labels={"UART0": "uart20", "UART1": "uart30"},
    # nrf54l_05_10_15.dtsi defines pwm20/pwm21/pwm22 -- same peripheral-instance-number
    # convention as uart_node_labels above, not nRF52840's pwm0-pwm3 low-number scheme.
    pwm_node_labels=["pwm20", "pwm21", "pwm22"],
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
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
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_overlay,
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
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

    # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
