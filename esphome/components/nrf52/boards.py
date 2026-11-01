from typing import Any

from esphome.components.zephyr import Section, zephyr_data
from esphome.components.zephyr.const import KEY_BOARD, KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE

from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
)


def is_default_sdk_version(framework_version: cv.Version) -> bool:
    """Return True for the SDK version BOARDS holds the data of."""
    default = cv.Version.parse(next(iter(BOARDS)))
    return (
        framework_version.major,
        framework_version.minor,
        framework_version.patch,
    ) == (
        default.major,
        default.minor,
        default.patch,
    )


def get_board_info(framework_version: cv.Version, board: str) -> dict[str, Any] | None:
    """Return the device tree data of a board, or None for a board that is not known.

    BOARDS holds the default SDK version. Another version, built with the sdk-nrf toolchain, uses
    the data read when ESPHome installed that SDK; until it is installed, and with the PlatformIO
    toolchain, the data of the default version is used.
    """
    if CORE.using_toolchain_sdk_nrf and not is_default_sdk_version(framework_version):
        from .framework import load_installed_board_data

        if (installed := load_installed_board_data(framework_version)) is not None:
            return installed.get(board)
    return next(iter(BOARDS.values())).get(board)


def get_current_board_info() -> dict[str, Any]:
    """Return the device tree data of the board being built, or {} for an unknown board."""
    framework_version: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    board = short_board_name(zephyr_data()[KEY_BOARD])
    return get_board_info(framework_version, board) or {}


def short_board_name(board: str) -> str:
    """Return <board> for <board>/<soc> when Zephyr also accepts the short name.

    Zephyr lets a board with a single SoC and no CPU clusters leave the SoC out, so
    xiao_ble/nrf52840 and xiao_ble are the same target. BOARDS lists such targets by
    the short name; any other name is returned unchanged. Use it to look the board up
    in ESPHome's tables only: west gets the board as the user wrote it.
    """
    name, _, soc = board.partition("/")
    if not soc or "/" in soc:
        return board
    for boards in BOARDS.values():
        if (info := boards.get(name)) is not None and info["variant"] == soc.upper():
            return name
    return board


BOARDS_ZEPHYR = {
    "adafruit_itsybitsy_nrf52840": {
        KEY_BOOTLOADER: [
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
            BOOTLOADER_ADAFRUIT,
            BOOTLOADER_ADAFRUIT_NRF52_SD132,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
        ]
    },
    "xiao_ble": {
        KEY_BOOTLOADER: [
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
            BOOTLOADER_ADAFRUIT,
            BOOTLOADER_ADAFRUIT_NRF52_SD132,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
        ]
    },
    "adafruit_itsybitsy": {
        KEY_BOOTLOADER: [
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
            BOOTLOADER_ADAFRUIT,
            BOOTLOADER_ADAFRUIT_NRF52_SD132,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
        ]
    },
    # Nordic nRF52840 Dongle (PCA10059): factory Open DFU bootloader at 0xE0000, kept in place
    "nrf52840dongle": {KEY_BOOTLOADER: [BOOTLOADER_NRF]},
}

# Boards with a GPIO wired to their RESET pin: driving it low is a pin reset, which the factory
# bootloader takes as its cue to stay in DFU mode (Nordic's own USB DFU trigger does the same)
SELF_RESET_PINS = {
    "nrf52840dongle": "P0.19",
}

# Bootloaders whose layout has a SoftDevice and the Adafruit bootloader, between which the mcumgr OTA
# image slot is placed
MCUMGR_OTA_BOOTLOADERS = (
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
)

# https://github.com/ffenix113/zigbee_home/blob/17bb7b9e9d375e756da9e38913f53303937fb66a/types/board/known_boards.go
# https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather?view=all#hathach-memory-map
BOOTLOADER_CONFIG = {
    BOOTLOADER_ADAFRUIT_NRF52_SD132: [
        Section("SoftDevice", 0x0, 0x26000, "flash_primary"),
        Section("Adafruit_nRF52_Bootloader", 0xF4000, 0xC000, "flash_primary"),
    ],
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: [
        Section("SoftDevice", 0x0, 0x26000, "flash_primary"),
        Section("Adafruit_nRF52_Bootloader", 0xF4000, 0xC000, "flash_primary"),
    ],
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: [
        Section("SoftDevice", 0x0, 0x27000, "flash_primary"),
        Section("Adafruit_nRF52_Bootloader", 0xF4000, 0xC000, "flash_primary"),
    ],
    # The board's BOARD_HAS_NRF5_BOOTLOADER already reserves the MBR page at 0x0, so the application
    # links at 0x1000, where the bootloader loads it; only the bootloader itself is reserved here
    BOOTLOADER_NRF: [
        Section("open_bootloader", 0xE0000, 0x20000, "flash_primary"),
    ],
}

"""
BOARDS generated with script/generate-nrf52-boards.py
"""

# nRF Connect SDK version -> board -> data read from the board device tree
BOARDS = {
    "2.9.2": {
        "96b_carbon/nrf51822": {
            "name": "Carbon",
            "variant": "NRF51822",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "96b_nitrogen": {
            "name": "Nitrogen",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "acn52832": {
            "name": "acn52832",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "adafruit_feather_nrf52840": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "adafruit_feather_nrf52840/nrf52840/sense": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "adafruit_feather_nrf52840/nrf52840/sense/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart0",
            ],
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "adafruit_feather_nrf52840/nrf52840/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart0",
            ],
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "adafruit_itsybitsy": {
            "name": "ItsyBitsy nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart0",
            ],
            "i2c": [
                "i2c0",
            ],
        },
        "arduino_nano_33_ble": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "arduino_nano_33_ble/nrf52840/sense": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "arduino_nicla_sense_me": {
            "name": "Arduino Nicla Sense ME",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bbc_microbit": {
            "name": "micro:bit",
            "variant": "NRF51822",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bbc_microbit_v2": {
            "name": "micro:bit V2",
            "variant": "NRF52833",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bl652_dvk": {
            "name": "BL652 DVK",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bl653_dvk": {
            "name": "BL653 DVK",
            "variant": "NRF52833",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bl654_dvk": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bl654_dvk/nrf52840/pa": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bl654_sensor_board": {
            "name": "BL654 Sensor Board",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bl654_usb": {
            "name": "BL654 USB (451-00004)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/bl654_cdc_acm_uart",
            ],
        },
        "blueclover_plt_demo_v2": {
            "name": "Blue Clover PLT Demo V2 nRF52832",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bt510": {
            "name": "Sentrius BT510 Sensor",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "bt610": {
            "name": "Sentrius BT610 Sensor",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "bytesensi_l": {
            "name": "byteSENSI-L",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "contextualelectronics_abc": {
            "name": "Advanced BLE Cell",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "croxel_cx1825": {
            "name": "CX1825 nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_0",
            ],
            "i2c": [
                "i2c0",
            ],
        },
        "ctcc": {
            "name": "Connectivity Card nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart",
            ],
        },
        "decawave_dwm1001_dev": {
            "name": "Decawave DWM1001",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "degu_evk": {
            "name": "Degu Evaluation Kit",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/degu_cdc_acm_uart",
            ],
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ebyte_e73_tbb": {
            "name": "E73-TBB",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "holyiot_yj16019": {
            "name": "YJ-16019",
            "variant": "NRF52832",
        },
        "mg100": {
            "name": "Sentrius™ MG100 Gateway",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "micromod": {
            "name": "MicroMod board Processor",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "nrf21540dk": {
            "name": "nRF21540 DK",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf51_ble400": {
            "name": "BLE400",
            "variant": "NRF51822",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf51_blenano": {
            "name": "Redbear Labs Nano",
            "variant": "NRF51822",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf51_vbluno51": {
            "name": "nRF51-VBLUno51",
            "variant": "NRF51822",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf51dk": {
            "name": "nRF51 DK",
            "variant": "NRF51822",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf51dongle": {
            "name": "nRF51 Dongle",
            "variant": "NRF51822",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52810dmouse": {
            "name": "nrf52810dmouse",
            "variant": "NRF52810",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52820dongle": {
            "name": "nrf52820dongle",
            "variant": "NRF52820",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52832_mdk": {
            "name": "nRF52832-mdk",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52833dk/nrf52820": {
            "name": "nRF52833 DK",
            "variant": "NRF52820",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52833dk/nrf52833": {
            "name": "nRF52833 DK",
            "variant": "NRF52833",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52833dongle": {
            "name": "nrf52833dongle",
            "variant": "NRF52833",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840_blip": {
            "name": "Labs Blip",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840_mdk": {
            "name": "nRF52840-mdk",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840_mdk_usb_dongle": {
            "name": "nRF52840 MDK USB Dongle",
            "variant": "NRF52840",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840_papyr": {
            "name": "Labs Papyr",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840dk/nrf52811": {
            "name": "nRF52840 DK",
            "variant": "NRF52811",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840dk/nrf52840": {
            "name": "nRF52840 DK",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840dongle": {
            "name": "nRF52840 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart",
            ],
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52840gmouse": {
            "name": "nrf52840gmouse",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52_adafruit_feather": {
            "name": "nRF52 Adafruit Feather",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52_blenano2": {
            "name": "Redbear Labs Nano v2",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52_sparkfun": {
            "name": "nRF52832 breakout",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52_vbluno52": {
            "name": "nRF52-VBLUno52",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52dk/nrf52805": {
            "name": "nRF52 DK",
            "variant": "NRF52805",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52dk/nrf52810": {
            "name": "nRF52 DK",
            "variant": "NRF52810",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52dk/nrf52832": {
            "name": "nRF52 DK",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52dmouse": {
            "name": "nrf52dmouse",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf52kbd": {
            "name": "nrf52kbd",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "nrf54h20dk/nrf54h20/cpuapp": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
            "uart": [
                "/soc/peripheral@5f000000/uart@9d5000",
            ],
        },
        "nrf54h20dk/nrf54h20/cpuflpr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
            "uart": [
                "/soc/peripheral@5f000000/uart@8e6000",
            ],
        },
        "nrf54h20dk/nrf54h20/cpuflpr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
            "uart": [
                "/soc/peripheral@5f000000/uart@8e6000",
            ],
        },
        "nrf54h20dk/nrf54h20/cpuppr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
            "uart": [
                "/soc/peripheral@5f000000/uart@9c6000",
            ],
        },
        "nrf54h20dk/nrf54h20/cpuppr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
            "uart": [
                "/soc/peripheral@5f000000/uart@9c6000",
            ],
        },
        "nrf54h20dk/nrf54h20/cpurad": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
            "uart": [
                "/soc/peripheral@5f000000/uart@9c6000",
            ],
        },
        "nrf54l15bsim/nrf54l15/cpuapp": {
            "name": "nRF54L15 simulated boards (BabbleSim)",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l05/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L05",
            "uart": [
                "/soc/peripheral@50000000/uart@c6000",
            ],
        },
        "nrf54l15dk/nrf54l10/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
            "uart": [
                "/soc/peripheral@50000000/uart@c6000",
            ],
        },
        "nrf54l15dk/nrf54l15/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
            "uart": [
                "/soc/peripheral@50000000/uart@c6000",
            ],
        },
        "nrf54l15dk/nrf54l15/cpuapp/ns": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
            "uart": [
                "/soc/peripheral@40000000/uart@c6000",
            ],
        },
        "nrf54l15dk/nrf54l15/cpuflpr": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
            "uart": [
                "/soc/peripheral@50000000/uart@104000",
            ],
        },
        "nrf54l15dk/nrf54l15/cpuflpr/xip": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
            "uart": [
                "/soc/peripheral@50000000/uart@104000",
            ],
        },
        "nrf54l20pdk/nrf54l20/cpuapp": {
            "name": "nRF54L20 PDK",
            "variant": "NRF54L20",
            "uart": [
                "/soc/peripheral@50000000/uart@c6000",
            ],
        },
        "nrf9160dk/nrf52840": {
            "name": "nRF9160 DK",
            "variant": "NRF52840",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "pan1770_evb": {
            "name": "PAN1770 Evaluation Board",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "pan1780_evb": {
            "name": "PAN1780 Evaluation Board",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "pan1781_evb": {
            "name": "PAN1781 Evaluation Board",
            "variant": "NRF52820",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "pan1782_evb": {
            "name": "PAN1782 Evaluation Board",
            "variant": "NRF52833",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "particle_argon": {
            "name": "Argon",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "particle_boron": {
            "name": "Boron",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "particle_xenon": {
            "name": "Xenon",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "pinetime_devkit0": {
            "name": "PineTime DevKit0",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "pinnacle_100_dvk": {
            "name": "Pinnacle 100 DVK",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "qemu_cortex_m0": {
            "name": "QEMU Emulation for ARM Cortex-M0",
            "variant": "NRF51822",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "rak4631": {
            "name": "RAK4631",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "rak5010": {
            "name": "RAK5010",
            "variant": "NRF52840",
            "i2c": [
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "raytac_mdbt50q_db_33": {
            "name": "MDBT50Q-DB-33",
            "variant": "NRF52833",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "raytac_mdbt50q_db_40": {
            "name": "MDBT50Q-DB-40",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "reel_board": {
            "name": "reel board",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "rm1xx_dvk": {
            "name": "RM1xx DVK",
            "variant": "NRF51822",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ruuvi_ruuvitag": {
            "name": "RuuviTag",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "thingy52": {
            "name": "Thingy:52",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "thingy91/nrf52840": {
            "name": "thingy91",
            "variant": "NRF52840",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_bmd300eval": {
            "name": "EVK-BMD-30/35: BMD-300-EVAL, BMD-301-EVAL, and BMD-350-EVAL",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_bmd330eval": {
            "name": "EVK-BMD-330: BMD-330-EVAL",
            "variant": "NRF52810",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_bmd340eval": {
            "name": "EVK-BMD-34/38: BMD-340-EVAL and BMD-341-EVAL",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
                "/soc/uart@40028000",
            ],
        },
        "ubx_bmd345eval": {
            "name": "EVK-BMD-34/38: BMD-345-EVAL",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_bmd360eval": {
            "name": "EVK-BMD-360: BMD-360-EVAL",
            "variant": "NRF52811",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_bmd380eval": {
            "name": "EVK-BMD-34/48: BMD-380-EVAL",
            "variant": "NRF52840",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_evkannab1": {
            "name": "EVK-ANNA-B11x",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_evkninab1": {
            "name": "EVK NINA-B11x",
            "variant": "NRF52832",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_evkninab3": {
            "name": "EVK-NINA-B3",
            "variant": "NRF52840",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "ubx_evkninab4": {
            "name": "EVK NINA-B40x",
            "variant": "NRF52833",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "we_ophelia1ev": {
            "name": "Ophelia-I EV NRF52805",
            "variant": "NRF52805",
            "i2c": [
                "i2c0",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "we_proteus2ev": {
            "name": "Proteus-II-EV",
            "variant": "NRF52832",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "we_proteus3ev": {
            "name": "Proteus-III-EV",
            "variant": "NRF52840",
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "xiao_ble": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc-acm-uart",
            ],
            "i2c": [
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
        "xiao_ble/nrf52840/sense": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc-acm-uart",
            ],
            "i2c": [
                "i2c0",
                "i2c1",
            ],
            "uart": [
                "/soc/uart@40002000",
            ],
        },
    },
}
# DO NOT ADD ANYTHING BELOW THIS LINE
