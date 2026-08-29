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
    ZEPHYR_VARIANT_NRF54LM20A,
)
from . import (
    MAINLINE,
    NCS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_DEFAULT_BOARD = "nrf54lm20dk"
# qualify_board() expands this to "nrf54lm20dk/nrf54lm20a/cpuapp" via soc=/qualifier=
# below -- the application core target confirmed in upstream Zephyr's own board.yml for
# this DK. Only the application core is supported here -- cpuflpr (the RISC-V
# co-processor core) is a separate, much more specialized target this variant does not
# build for.

_ADVANCED_SCHEMA = ADVANCED_SCHEMA

# GPIO -> nRF54LM20A SAADC analog-input name. Fixed silicon fact (AIN0-AIN7 datasheet
# pin assignment) -- not discoverable from any board's DTS, same reasoning as nrf52's
# own _ADC_AIN_MAP. Cross-checked against Nordic's nRF54LM20A/nRF54LM20B datasheet
# v1.0 (QFN52 pinout table): P1.00/AIN0, P1.31/AIN1, P1.30/AIN2, P1.29/AIN3, P1.06/AIN4,
# P1.05/AIN5, P1.04/AIN6, P1.03/AIN7. Non-monotonic pin order is a real silicon fact,
# unlike nrf54l15's contiguous AIN0-AIN7 -- confirmed directly against the datasheet's
# pin assignment table, not just the pinout diagram.
# Unlike nrf52 (single GPIO port, so pin number == AIN's flat GPIO number directly),
# these pins are all on P1 -- flat number is port * gpio_port_width(32) + pin, e.g.
# P1.00 -> 32.
_ADC_AIN_MAP = {
    32: "AIN0",  # P1.00
    63: "AIN1",  # P1.31
    62: "AIN2",  # P1.30
    61: "AIN3",  # P1.29
    38: "AIN4",  # P1.06
    37: "AIN5",  # P1.05
    36: "AIN6",  # P1.04
    35: "AIN7",  # P1.03
}

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NRF54LM20A
VARIANT = ZephyrVariant(
    # Same reasoning as nrf52/nrf54l15: NCS is Nordic's own SDK and where support for its
    # own newest silicon lands and gets tested first -- nrf54lm20dk is already in
    # nrfconnect/sdk-zephyr today. Mainline Zephyr (which also carries this SoC, plus
    # boards NCS doesn't have yet such as the Seeed xiao_nrf54lm20a) stays available as
    # an alternate.
    sdk=NCS,
    sdk_name="ncs",
    alt_sdks={"zephyr": MAINLINE},
    family="nordic",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # Untested on hardware as of 2026-08-04.
    transports=frozenset({"ble", "openthread", "zigbee"}),
    soc="nrf54lm20a",
    qualifier="cpuapp",
    # No "scratch": neither board (nrf54lm20dk or xiao_nrf54lm20a) defines a
    # scratch_partition, and move/offset don't need one.
    # offset is untested on hardware as of 2026-08-02, update this comment when tested.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    # nrf54lm20_a_b.dtsi (SoC-level) defines uart20-uart24; there is no uart0/uart1 node
    # on this SoC. uart20 is nrf54lm20dk's own console (VCOM0); uart21 isn't pinctrl'd by
    # the DK board itself but is a real SoC instance (e.g. wired to header pins on the
    # Seeed xiao_nrf54lm20a board).
    uart_node_labels={"UART0": "uart20", "UART1": "uart21"},
    # nrf54lm20_a_b.dtsi defines pwm20/pwm21/pwm22 -- same peripheral-instance-number
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
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NRF54LM20A")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF54LM20A")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    # Unlike nrf54l15, this SoC's boards already alias `watchdog0` to &wdt31
    # (e.g. nrf54lm20dk_common.dtsi) -- they just leave the peripheral itself disabled
    # by default, same as the bare SoC dtsi. WDT30 is reserved for the Secure domain on
    # this SoC; WDT31 is the one available to the application core we build for here,
    # so only the `status` needs setting.
    zephyr_add_overlay(
        """
        &wdt31 {
            status = "okay";
        };
        """
    )

    # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
