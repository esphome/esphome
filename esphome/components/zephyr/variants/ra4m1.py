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
    BOOTLOADER_SCHEMA,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    KEY_BOOTLOADER,
    ZEPHYR_VARIANT_RA4M1,
)
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# RA4M1 is single-core -- soc= is left unset (that field exists to disambiguate
# multi-core chips like esp32_c6's HP/LP split, not to pin a single package variant;
# each supported board's own board.yml already declares its one real SoC package).
_DEFAULT_BOARD = "ek_ra4m1"

# ek_ra4m1's stock board has no slot0/slot1 partitions -- MCUboot is opt-in, same
# shape as rp2040.py: anyone choosing it supplies their own fully-qualified board and
# MCUboot-shaped partition overlay.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(BOOTLOADER_SCHEMA)

# GPIO -> RA4M1 ADC channel name, from Renesas' RA4M1 Group Datasheet's Pin Lists
# (100-pin LQFP column).
_ADC_AIN_MAP = {
    0: "AN000",
    1: "AN001",
    2: "AN002",
    3: "AN003",
    4: "AN004",
    5: "AN011",
    6: "AN012",
    7: "AN013",
    8: "AN014",
    16: "AN005",
    17: "AN006",
    18: "AN007",
    19: "AN008",
    20: "AN009",
    21: "AN010",
    256: "AN022",
    257: "AN021",
    258: "AN020",
    259: "AN019",
    1280: "AN016",
    1281: "AN017",
    1282: "AN018",
    1283: "AN023",
    1284: "AN024",
    1285: "AN025",
}

# GPIO -> (index into pwm_node_labels, local channel A=0/B=1) for RA4M1's GPT-based
# PWM hardware. Only GTIOC1A/GTIOC1B -- port4 pin5 and port4 pin6, i.e. flat pin
# numbers 4*16+5=69 and 4*16+6=70 under this variant's gpio_port_width=16 encoding
# -- are wired to pwm1, the only PWM instance this codebase enables (see
# pwm_node_labels below). Matches ek_ra4m1's own default pinctrl
# (RA_PSEL(RA_PSEL_GPT1, 4, 5) / RA_PSEL(RA_PSEL_GPT1, 4, 6)); Zephyr's own
# pinctrl-ra.h only defines RA_PSEL_GPT0/GPT1 in this SDK version, so no other GPT
# channel is routable yet regardless of pin.
_PWM_PIN_MAP = {
    69: (0, 0),
    70: (0, 1),
}

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_RA4M1
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="renesas",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=16,
    # RA4M1's GPIO port nodes are labelled ioport0/ioport1/... in Zephyr's own dtsi
    # (renesas,ra-gpio-ioport), not the gpio0/gpio1 convention every other variant uses.
    gpio_node_prefix="ioport",
    # RA4M1's WDT is clocked from pclkb (24MHz on every board shipped so far -- HOCO
    # 48MHz / pclkb div=2). The driver's own largest divisor x cycle-count combination
    # (8192 x 16384) only reaches ~5.6s at that clock regardless of what's requested;
    # 5000ms coincides with the schema's own 5s floor, so this variant only supports
    # the minimum requestable value.
    watchdog_max_timeout_ms=5000,
    # Explicitly empty, not the old hardcoded {"UART0": "uart1", "UART1": "uart2"}:
    # that held only for ek_ra4m1 (console sci1/uart1) -- arduino_uno_r4's console is
    # sci2/uart2 instead, which would have made UART0 (always the board's console, by
    # convention) wrong there. Empty tells resolve_uart_node_label() (dts_lookup.py)
    # to resolve hardware_uart per board.
    uart_node_labels={},
    adc_ain_map=_ADC_AIN_MAP,
    # Only pwm1 is enabled in ek_ra4m1.dts.
    pwm_node_labels=["pwm1"],
    pwm_channels_per_block=2,
    pwm_pin_map=_PWM_PIN_MAP,
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    bootloader = config[CONF_ADVANCED][CONF_BOOTLOADER]
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "ra4m1", config, "RA4M1 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        bootloader if bootloader == BOOTLOADER_MCUBOOT else "",
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_data,
        zephyr_setup_preferences,
        zephyr_to_code,
    )

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_RA4M1")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RA4M1")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")
