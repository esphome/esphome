import esphome.codegen as cg
import esphome.config_validation as cv
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
    BOOTLOADER_MCUBOOT,
    BOOTLOADER_NONE,
    CONF_BOOTLOADER,
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
_ADVANCED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_NONE): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

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
    # UART0 -> ek_ra4m1's own sci1/uart1 (the default board); UART1 -> the
    # arduino_nano_r4 board's sci2/uart2 -- same shape as nRF54's own multi-entry
    # uart_node_labels, here spanning two different boards on this variant rather than
    # two simultaneously-present peripherals on one board. A board other than the
    # default must pick whichever UART name maps to its own real console UART via
    # `logger: hardware_uart:`.
    uart_node_labels={"UART0": "uart1", "UART1": "uart2"},
    adc_ain_map=_ADC_AIN_MAP,
    # Only pwm1 is enabled in ek_ra4m1.dts.
    pwm_node_labels=["pwm1"],
)

# Board -> (app-mode VID:PID, DFU-mode VID:PID), for boards flashed via
# Arduino's own dfu-util fork (adds a -Q quirks flag stock dfu-util doesn't
# have) using its dual-VID:PID auto-detach form, rather than west's generic
# single-VID:PID dfu-util runner. Confirmed against Arduino's own boards.txt
# (nanor4.upload_port.0/.1) -- not derivable from board.cmake/runners.yaml,
# which only knows the DFU-mode PID.
_ARDUINO_DFU_BOARDS = {
    "arduino_nano_r4": ("0x2341:0x0074", ":0x0374"),
}


def arduino_dfu_pids(board: str) -> tuple[str, str] | None:
    return _ARDUINO_DFU_BOARDS.get(board)


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
