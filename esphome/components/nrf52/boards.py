from typing import Any

from esphome.components.zephyr import Section
from esphome.components.zephyr.const import KEY_BOOTLOADER
import esphome.config_validation as cv

from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
)


def get_board_info(framework_version: cv.Version, board: str) -> dict[str, Any] | None:
    """Return the device tree data of a board, or None for a board that is not known.

    The data comes from the newest SDK version in BOARDS that is not newer than
    framework_version, or the oldest one for an older framework.
    """
    versions = sorted(BOARDS, key=cv.Version.parse)
    version = versions[0]
    for candidate in versions:
        if cv.Version.parse(candidate) <= framework_version:
            version = candidate
    return BOARDS[version].get(board)


def short_board_name(board: str) -> str:
    """Return <board> for <board>/<soc> when Zephyr also accepts the short name.

    Zephyr lets a board with a single SoC and no CPU clusters leave the SoC out, so
    xiao_ble/nrf52840 and xiao_ble are the same target. BOARDS lists such targets by
    the short name; any other name is returned unchanged.
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
        },
        "96b_nitrogen": {
            "name": "Nitrogen",
            "variant": "NRF52832",
        },
        "acn52832": {
            "name": "acn52832",
            "variant": "NRF52832",
        },
        "adafruit_feather_nrf52840": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
        },
        "adafruit_feather_nrf52840/nrf52840/sense": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
        },
        "adafruit_feather_nrf52840/nrf52840/sense/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart0",
            ],
        },
        "adafruit_feather_nrf52840/nrf52840/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart0",
            ],
        },
        "adafruit_itsybitsy": {
            "name": "ItsyBitsy nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart0",
            ],
        },
        "arduino_nano_33_ble": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
        },
        "arduino_nano_33_ble/nrf52840/sense": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
        },
        "arduino_nicla_sense_me": {
            "name": "Arduino Nicla Sense ME",
            "variant": "NRF52832",
        },
        "bbc_microbit": {
            "name": "micro:bit",
            "variant": "NRF51822",
        },
        "bbc_microbit_v2": {
            "name": "micro:bit V2",
            "variant": "NRF52833",
        },
        "bl652_dvk": {
            "name": "BL652 DVK",
            "variant": "NRF52832",
        },
        "bl653_dvk": {
            "name": "BL653 DVK",
            "variant": "NRF52833",
        },
        "bl654_dvk": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
        },
        "bl654_dvk/nrf52840/pa": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
        },
        "bl654_sensor_board": {
            "name": "BL654 Sensor Board",
            "variant": "NRF52840",
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
        },
        "bt510": {
            "name": "Sentrius BT510 Sensor",
            "variant": "NRF52840",
        },
        "bt610": {
            "name": "Sentrius BT610 Sensor",
            "variant": "NRF52840",
        },
        "bytesensi_l": {
            "name": "byteSENSI-L",
            "variant": "NRF52832",
        },
        "contextualelectronics_abc": {
            "name": "Advanced BLE Cell",
            "variant": "NRF52840",
        },
        "croxel_cx1825": {
            "name": "CX1825 nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_0",
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
        },
        "degu_evk": {
            "name": "Degu Evaluation Kit",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/degu_cdc_acm_uart",
            ],
        },
        "ebyte_e73_tbb": {
            "name": "E73-TBB",
            "variant": "NRF52832",
        },
        "holyiot_yj16019": {
            "name": "YJ-16019",
            "variant": "NRF52832",
        },
        "mg100": {
            "name": "Sentrius™ MG100 Gateway",
            "variant": "NRF52840",
        },
        "micromod": {
            "name": "MicroMod board Processor",
            "variant": "NRF52840",
        },
        "nrf21540dk": {
            "name": "nRF21540 DK",
            "variant": "NRF52840",
        },
        "nrf51_ble400": {
            "name": "BLE400",
            "variant": "NRF51822",
        },
        "nrf51_blenano": {
            "name": "Redbear Labs Nano",
            "variant": "NRF51822",
        },
        "nrf51_vbluno51": {
            "name": "nRF51-VBLUno51",
            "variant": "NRF51822",
        },
        "nrf51dk": {
            "name": "nRF51 DK",
            "variant": "NRF51822",
        },
        "nrf51dongle": {
            "name": "nRF51 Dongle",
            "variant": "NRF51822",
        },
        "nrf52810dmouse": {
            "name": "nrf52810dmouse",
            "variant": "NRF52810",
        },
        "nrf52820dongle": {
            "name": "nrf52820dongle",
            "variant": "NRF52820",
        },
        "nrf52832_mdk": {
            "name": "nRF52832-mdk",
            "variant": "NRF52832",
        },
        "nrf52833dk/nrf52820": {
            "name": "nRF52833 DK",
            "variant": "NRF52820",
        },
        "nrf52833dk/nrf52833": {
            "name": "nRF52833 DK",
            "variant": "NRF52833",
        },
        "nrf52833dongle": {
            "name": "nrf52833dongle",
            "variant": "NRF52833",
        },
        "nrf52840_blip": {
            "name": "Labs Blip",
            "variant": "NRF52840",
        },
        "nrf52840_mdk": {
            "name": "nRF52840-mdk",
            "variant": "NRF52840",
        },
        "nrf52840_mdk_usb_dongle": {
            "name": "nRF52840 MDK USB Dongle",
            "variant": "NRF52840",
        },
        "nrf52840_papyr": {
            "name": "Labs Papyr",
            "variant": "NRF52840",
        },
        "nrf52840dk/nrf52811": {
            "name": "nRF52840 DK",
            "variant": "NRF52811",
        },
        "nrf52840dk/nrf52840": {
            "name": "nRF52840 DK",
            "variant": "NRF52840",
        },
        "nrf52840dongle": {
            "name": "nRF52840 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc_acm_uart",
            ],
        },
        "nrf52840gmouse": {
            "name": "nrf52840gmouse",
            "variant": "NRF52840",
        },
        "nrf52_adafruit_feather": {
            "name": "nRF52 Adafruit Feather",
            "variant": "NRF52832",
        },
        "nrf52_blenano2": {
            "name": "Redbear Labs Nano v2",
            "variant": "NRF52832",
        },
        "nrf52_sparkfun": {
            "name": "nRF52832 breakout",
            "variant": "NRF52832",
        },
        "nrf52_vbluno52": {
            "name": "nRF52-VBLUno52",
            "variant": "NRF52832",
        },
        "nrf52dk/nrf52805": {
            "name": "nRF52 DK",
            "variant": "NRF52805",
        },
        "nrf52dk/nrf52810": {
            "name": "nRF52 DK",
            "variant": "NRF52810",
        },
        "nrf52dk/nrf52832": {
            "name": "nRF52 DK",
            "variant": "NRF52832",
        },
        "nrf52dmouse": {
            "name": "nrf52dmouse",
            "variant": "NRF52832",
        },
        "nrf52kbd": {
            "name": "nrf52kbd",
            "variant": "NRF52832",
        },
        "nrf54h20dk/nrf54h20/cpuapp": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuflpr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuflpr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuppr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuppr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpurad": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54l15bsim/nrf54l15/cpuapp": {
            "name": "nRF54L15 simulated boards (BabbleSim)",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l05/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L05",
        },
        "nrf54l15dk/nrf54l10/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
        },
        "nrf54l15dk/nrf54l15/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuapp/ns": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuflpr": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuflpr/xip": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l20pdk/nrf54l20/cpuapp": {
            "name": "nRF54L20 PDK",
            "variant": "NRF54L20",
        },
        "nrf9160dk/nrf52840": {
            "name": "nRF9160 DK",
            "variant": "NRF52840",
        },
        "pan1770_evb": {
            "name": "PAN1770 Evaluation Board",
            "variant": "NRF52840",
        },
        "pan1780_evb": {
            "name": "PAN1780 Evaluation Board",
            "variant": "NRF52840",
        },
        "pan1781_evb": {
            "name": "PAN1781 Evaluation Board",
            "variant": "NRF52820",
        },
        "pan1782_evb": {
            "name": "PAN1782 Evaluation Board",
            "variant": "NRF52833",
        },
        "particle_argon": {
            "name": "Argon",
            "variant": "NRF52840",
        },
        "particle_boron": {
            "name": "Boron",
            "variant": "NRF52840",
        },
        "particle_xenon": {
            "name": "Xenon",
            "variant": "NRF52840",
        },
        "pinetime_devkit0": {
            "name": "PineTime DevKit0",
            "variant": "NRF52832",
        },
        "pinnacle_100_dvk": {
            "name": "Pinnacle 100 DVK",
            "variant": "NRF52840",
        },
        "qemu_cortex_m0": {
            "name": "QEMU Emulation for ARM Cortex-M0",
            "variant": "NRF51822",
        },
        "rak4631": {
            "name": "RAK4631",
            "variant": "NRF52840",
        },
        "rak5010": {
            "name": "RAK5010",
            "variant": "NRF52840",
        },
        "raytac_mdbt50q_db_33": {
            "name": "MDBT50Q-DB-33",
            "variant": "NRF52833",
        },
        "raytac_mdbt50q_db_40": {
            "name": "MDBT50Q-DB-40",
            "variant": "NRF52840",
        },
        "reel_board": {
            "name": "reel board",
            "variant": "NRF52840",
        },
        "rm1xx_dvk": {
            "name": "RM1xx DVK",
            "variant": "NRF51822",
        },
        "ruuvi_ruuvitag": {
            "name": "RuuviTag",
            "variant": "NRF52832",
        },
        "thingy52": {
            "name": "Thingy:52",
            "variant": "NRF52832",
        },
        "thingy91/nrf52840": {
            "name": "thingy91",
            "variant": "NRF52840",
        },
        "ubx_bmd300eval": {
            "name": "EVK-BMD-30/35: BMD-300-EVAL, BMD-301-EVAL, and BMD-350-EVAL",
            "variant": "NRF52832",
        },
        "ubx_bmd330eval": {
            "name": "EVK-BMD-330: BMD-330-EVAL",
            "variant": "NRF52810",
        },
        "ubx_bmd340eval": {
            "name": "EVK-BMD-34/38: BMD-340-EVAL and BMD-341-EVAL",
            "variant": "NRF52840",
        },
        "ubx_bmd345eval": {
            "name": "EVK-BMD-34/38: BMD-345-EVAL",
            "variant": "NRF52840",
        },
        "ubx_bmd360eval": {
            "name": "EVK-BMD-360: BMD-360-EVAL",
            "variant": "NRF52811",
        },
        "ubx_bmd380eval": {
            "name": "EVK-BMD-34/48: BMD-380-EVAL",
            "variant": "NRF52840",
        },
        "ubx_evkannab1": {
            "name": "EVK-ANNA-B11x",
            "variant": "NRF52832",
        },
        "ubx_evkninab1": {
            "name": "EVK NINA-B11x",
            "variant": "NRF52832",
        },
        "ubx_evkninab3": {
            "name": "EVK-NINA-B3",
            "variant": "NRF52840",
        },
        "ubx_evkninab4": {
            "name": "EVK NINA-B40x",
            "variant": "NRF52833",
        },
        "we_ophelia1ev": {
            "name": "Ophelia-I EV NRF52805",
            "variant": "NRF52805",
        },
        "we_proteus2ev": {
            "name": "Proteus-II-EV",
            "variant": "NRF52832",
        },
        "we_proteus3ev": {
            "name": "Proteus-III-EV",
            "variant": "NRF52840",
        },
        "xiao_ble": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc-acm-uart",
            ],
        },
        "xiao_ble/nrf52840/sense": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/cdc-acm-uart",
            ],
        },
    },
    "3.2.0": {
        "96b_carbon/nrf51822": {
            "name": "Carbon",
            "variant": "NRF51822",
        },
        "96b_nitrogen": {
            "name": "Nitrogen",
            "variant": "NRF52832",
        },
        "acn52832": {
            "name": "acn52832",
            "variant": "NRF52832",
        },
        "adafruit_feather_nrf52840": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
        },
        "adafruit_feather_nrf52840/nrf52840/sense": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
        },
        "adafruit_feather_nrf52840/nrf52840/sense/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "adafruit_feather_nrf52840/nrf52840/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "adafruit_itsybitsy": {
            "name": "ItsyBitsy nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "arduino_nano_33_ble": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
        },
        "arduino_nano_33_ble/nrf52840/sense": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
        },
        "arduino_nicla_sense_me": {
            "name": "Arduino Nicla Sense ME",
            "variant": "NRF52832",
        },
        "bbc_microbit": {
            "name": "micro:bit",
            "variant": "NRF51822",
        },
        "bbc_microbit_v2": {
            "name": "micro:bit V2",
            "variant": "NRF52833",
        },
        "bl54l15_dvk/nrf54l10/cpuapp": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L10",
        },
        "bl54l15_dvk/nrf54l10/cpuapp/ns": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L10",
        },
        "bl54l15_dvk/nrf54l15/cpuapp": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15_dvk/nrf54l15/cpuapp/ns": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15_dvk/nrf54l15/cpuflpr": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15_dvk/nrf54l15/cpuflpr/xip": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuapp": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuapp/ns": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuflpr": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuflpr/xip": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl652_dvk": {
            "name": "BL652 DVK",
            "variant": "NRF52832",
        },
        "bl653_dvk": {
            "name": "BL653 DVK",
            "variant": "NRF52833",
        },
        "bl654_dvk": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
        },
        "bl654_dvk/nrf52840/pa": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
        },
        "bl654_sensor_board": {
            "name": "BL654 Sensor Board",
            "variant": "NRF52840",
        },
        "bl654_usb": {
            "name": "BL654 USB (451-00004)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "bl654_usb/nrf52840/bare": {
            "name": "BL654 USB (451-00004)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "blueclover_plt_demo_v2": {
            "name": "Blue Clover PLT Demo V2 nRF52832",
            "variant": "NRF52832",
        },
        "bt510": {
            "name": "Sentrius BT510 Sensor",
            "variant": "NRF52840",
        },
        "bt610": {
            "name": "Sentrius BT610 Sensor",
            "variant": "NRF52840",
        },
        "bytesensi_l": {
            "name": "byteSENSI-L",
            "variant": "NRF52832",
        },
        "contextualelectronics_abc": {
            "name": "Advanced BLE Cell",
            "variant": "NRF52840",
        },
        "croxel_cx1825": {
            "name": "CX1825 nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "ctcc/nrf52840": {
            "name": "CTHINGS.CO Connectivity Card",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "decawave_dwm1001_dev": {
            "name": "Decawave DWM1001",
            "variant": "NRF52832",
        },
        "decawave_dwm3001cdk": {
            "name": "Decawave DWM3001CDK",
            "variant": "NRF52833",
        },
        "degu_evk": {
            "name": "Degu Evaluation Kit",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "ebyte_e73_tbb": {
            "name": "E73-TBB",
            "variant": "NRF52832",
        },
        "holyiot_yj16019": {
            "name": "YJ-16019",
            "variant": "NRF52832",
        },
        "holyiot_yj17095": {
            "name": "YJ-17095",
            "variant": "NRF52832",
        },
        "mg100": {
            "name": "Sentrius™ MG100 Gateway",
            "variant": "NRF52840",
        },
        "micromod": {
            "name": "MicroMod board Processor",
            "variant": "NRF52840",
        },
        "nrf21540dk": {
            "name": "nRF21540 DK",
            "variant": "NRF52840",
        },
        "nrf51_ble400": {
            "name": "BLE400",
            "variant": "NRF51822",
        },
        "nrf51_blenano": {
            "name": "Redbear Labs Nano",
            "variant": "NRF51822",
        },
        "nrf51_vbluno51": {
            "name": "nRF51-VBLUno51",
            "variant": "NRF51822",
        },
        "nrf51dk": {
            "name": "nRF51 DK",
            "variant": "NRF51822",
        },
        "nrf51dongle": {
            "name": "nRF51 Dongle",
            "variant": "NRF51822",
        },
        "nrf52820dongle": {
            "name": "nrf52820dongle",
            "variant": "NRF52820",
        },
        "nrf52832_mdk": {
            "name": "nRF52832-mdk",
            "variant": "NRF52832",
        },
        "nrf52833dk/nrf52820": {
            "name": "nRF52833 DK",
            "variant": "NRF52820",
        },
        "nrf52833dk/nrf52833": {
            "name": "nRF52833 DK",
            "variant": "NRF52833",
        },
        "nrf52833dongle": {
            "name": "nrf52833dongle",
            "variant": "NRF52833",
        },
        "nrf52840_blip": {
            "name": "Labs Blip",
            "variant": "NRF52840",
        },
        "nrf52840_mdk": {
            "name": "nRF52840-mdk",
            "variant": "NRF52840",
        },
        "nrf52840_mdk_usb_dongle": {
            "name": "nRF52840 MDK USB Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "nrf52840_papyr": {
            "name": "Labs Papyr",
            "variant": "NRF52840",
        },
        "nrf52840dk/nrf52811": {
            "name": "nRF52840 DK",
            "variant": "NRF52811",
        },
        "nrf52840dk/nrf52840": {
            "name": "nRF52840 DK",
            "variant": "NRF52840",
        },
        "nrf52840dongle": {
            "name": "nRF52840 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "nrf52840dongle/nrf52840/bare": {
            "name": "nRF52840 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "nrf52840gmouse": {
            "name": "nrf52840gmouse",
            "variant": "NRF52840",
        },
        "nrf52_adafruit_feather": {
            "name": "nRF52 Adafruit Feather",
            "variant": "NRF52832",
        },
        "nrf52_blenano2": {
            "name": "Redbear Labs Nano v2",
            "variant": "NRF52832",
        },
        "nrf52_sparkfun": {
            "name": "nRF52832 breakout",
            "variant": "NRF52832",
        },
        "nrf52_vbluno52": {
            "name": "nRF52-VBLUno52",
            "variant": "NRF52832",
        },
        "nrf52dk/nrf52805": {
            "name": "nRF52 DK",
            "variant": "NRF52805",
        },
        "nrf52dk/nrf52810": {
            "name": "nRF52 DK",
            "variant": "NRF52810",
        },
        "nrf52dk/nrf52832": {
            "name": "nRF52 DK",
            "variant": "NRF52832",
        },
        "nrf52dmouse": {
            "name": "nrf52dmouse",
            "variant": "NRF52832",
        },
        "nrf52kbd": {
            "name": "nrf52kbd",
            "variant": "NRF52832",
        },
        "nrf54h20dk/nrf54h20/cpuapp": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuflpr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuflpr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuppr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuppr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpurad": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54l15bsim/nrf54l15/cpuapp": {
            "name": "nRF54L15 simulated boards (BabbleSim)",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l05/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L05",
        },
        "nrf54l15dk/nrf54l10/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
        },
        "nrf54l15dk/nrf54l10/cpuapp/ns": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
        },
        "nrf54l15dk/nrf54l15/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuapp/ns": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuflpr": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuflpr/xip": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54lm20bsim/nrf54lm20a/cpuapp": {
            "name": "nRF54LM20 simulated boards (BabbleSim)",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20a/cpuapp": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20a/cpuapp/ns": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20a/cpuflpr": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20A",
        },
        "nrf54ls05dk/nrf54ls05b/cpuapp": {
            "name": "nRF54LS05B DK",
            "variant": "NRF54LS05B",
        },
        "nrf54lv10dk/nrf54lv10a/cpuapp": {
            "name": "nRF54LV10 DK",
            "variant": "NRF54LV10A",
        },
        "nrf54lv10dk/nrf54lv10a/cpuapp/ns": {
            "name": "nRF54LV10 DK",
            "variant": "NRF54LV10A",
        },
        "nrf54lv10dk/nrf54lv10a/cpuflpr": {
            "name": "nRF54LV10 DK",
            "variant": "NRF54LV10A",
        },
        "nrf9160dk/nrf52840": {
            "name": "nRF9160 DK",
            "variant": "NRF52840",
        },
        "ophelia4ev/nrf54l15/cpuapp": {
            "name": "Ophelia-IV DK",
            "variant": "NRF54L15",
        },
        "ophelia4ev/nrf54l15/cpuflpr": {
            "name": "Ophelia-IV DK",
            "variant": "NRF54L15",
        },
        "pan1770_evb": {
            "name": "PAN1770 Evaluation Board",
            "variant": "NRF52840",
        },
        "pan1780_evb": {
            "name": "PAN1780 Evaluation Board",
            "variant": "NRF52840",
        },
        "pan1781_evb": {
            "name": "PAN1781 Evaluation Board",
            "variant": "NRF52820",
        },
        "pan1782_evb": {
            "name": "PAN1782 Evaluation Board",
            "variant": "NRF52833",
        },
        "panb611evb/nrf54l15/cpuapp": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "panb611evb/nrf54l15/cpuapp/ns": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "panb611evb/nrf54l15/cpuflpr": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "panb611evb/nrf54l15/cpuflpr/xip": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "particle_argon": {
            "name": "Argon",
            "variant": "NRF52840",
        },
        "particle_boron": {
            "name": "Boron",
            "variant": "NRF52840",
        },
        "particle_xenon": {
            "name": "Xenon",
            "variant": "NRF52840",
        },
        "pinetime_devkit0": {
            "name": "PineTime DevKit0",
            "variant": "NRF52832",
        },
        "pinnacle_100_dvk": {
            "name": "Pinnacle 100 DVK",
            "variant": "NRF52840",
        },
        "promicro_nrf52840": {
            "name": "Pro Micro nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "promicro_nrf52840/nrf52840/uf2": {
            "name": "Pro Micro nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "qemu_cortex_m0": {
            "name": "QEMU Emulation for ARM Cortex-M0",
            "variant": "NRF51822",
        },
        "quill_nrf52840_mesh": {
            "name": "Quill nRF52840 Mesh",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "rak4631": {
            "name": "RAK4631",
            "variant": "NRF52840",
        },
        "rak5010": {
            "name": "RAK5010",
            "variant": "NRF52840",
        },
        "raytac_an54l15q_db/nrf54l15/cpuapp": {
            "name": "AN54L15Q-DB",
            "variant": "NRF54L15",
        },
        "raytac_an54l15q_db/nrf54l15/cpuapp/ns": {
            "name": "AN54L15Q-DB",
            "variant": "NRF54L15",
        },
        "raytac_an54l15q_db/nrf54l15/cpuflpr": {
            "name": "AN54L15Q-DB",
            "variant": "NRF54L15",
        },
        "raytac_an54l15q_db/nrf54l15/cpuflpr/xip": {
            "name": "AN54L15Q-DB",
            "variant": "NRF54L15",
        },
        "raytac_mdbt50q_cx_40_dongle": {
            "name": "MDBT50Q-CX-40 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "raytac_mdbt50q_db_33": {
            "name": "MDBT50Q-DB-33",
            "variant": "NRF52833",
        },
        "raytac_mdbt50q_db_40": {
            "name": "MDBT50Q-DB-40",
            "variant": "NRF52840",
        },
        "reel_board": {
            "name": "reel board",
            "variant": "NRF52840",
        },
        "rm1xx_dvk": {
            "name": "RM1xx DVK",
            "variant": "NRF51822",
        },
        "ruuvi_ruuvitag": {
            "name": "RuuviTag",
            "variant": "NRF52832",
        },
        "thingy52": {
            "name": "Thingy:52",
            "variant": "NRF52832",
        },
        "thingy91/nrf52840": {
            "name": "thingy91",
            "variant": "NRF52840",
        },
        "ubx_bmd300eval": {
            "name": "EVK-BMD-30/35: BMD-300-EVAL, BMD-301-EVAL, and BMD-350-EVAL",
            "variant": "NRF52832",
        },
        "ubx_bmd330eval": {
            "name": "EVK-BMD-330: BMD-330-EVAL",
            "variant": "NRF52810",
        },
        "ubx_bmd340eval": {
            "name": "EVK-BMD-34/38: BMD-340-EVAL and BMD-341-EVAL",
            "variant": "NRF52840",
        },
        "ubx_bmd345eval": {
            "name": "EVK-BMD-34/38: BMD-345-EVAL",
            "variant": "NRF52840",
        },
        "ubx_bmd360eval": {
            "name": "EVK-BMD-360: BMD-360-EVAL",
            "variant": "NRF52811",
        },
        "ubx_bmd380eval": {
            "name": "EVK-BMD-34/48: BMD-380-EVAL",
            "variant": "NRF52840",
        },
        "ubx_evkannab1": {
            "name": "EVK-ANNA-B11x",
            "variant": "NRF52832",
        },
        "ubx_evkninab1": {
            "name": "EVK NINA-B11x",
            "variant": "NRF52832",
        },
        "ubx_evkninab3": {
            "name": "EVK-NINA-B3",
            "variant": "NRF52840",
        },
        "ubx_evkninab4": {
            "name": "EVK NINA-B40x",
            "variant": "NRF52833",
        },
        "we_ophelia1ev": {
            "name": "Ophelia-I EV NRF52805",
            "variant": "NRF52805",
        },
        "we_proteus2ev": {
            "name": "Proteus-II-EV",
            "variant": "NRF52832",
        },
        "we_proteus3ev": {
            "name": "Proteus-III-EV",
            "variant": "NRF52840",
        },
        "wio_wm1110_dev_kit": {
            "name": "Wio-WM1110 Dev Kit",
            "variant": "NRF52840",
        },
        "xiao_ble": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "xiao_ble/nrf52840/sense": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "xiao_nrf54l15/nrf54l15/cpuapp": {
            "name": "XIAO NRF54L15",
            "variant": "NRF54L15",
        },
        "xiao_nrf54l15/nrf54l15/cpuflpr": {
            "name": "XIAO NRF54L15",
            "variant": "NRF54L15",
        },
    },
    "3.4.0": {
        "96b_carbon/nrf51822": {
            "name": "Carbon",
            "variant": "NRF51822",
        },
        "96b_nitrogen": {
            "name": "Nitrogen",
            "variant": "NRF52832",
        },
        "acn52832": {
            "name": "acn52832",
            "variant": "NRF52832",
        },
        "adafruit_feather_nrf52840": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
        },
        "adafruit_feather_nrf52840/nrf52840/sense": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
        },
        "adafruit_feather_nrf52840/nrf52840/sense/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "adafruit_feather_nrf52840/nrf52840/uf2": {
            "name": "Feather nRF52840 (Express, Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "adafruit_itsybitsy": {
            "name": "ItsyBitsy nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "arduino_nano_33_ble": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
        },
        "arduino_nano_33_ble/nrf52840/sense": {
            "name": "Arduino Nano 33 BLE (Sense)",
            "variant": "NRF52840",
        },
        "arduino_nicla_sense_me": {
            "name": "Arduino Nicla Sense ME",
            "variant": "NRF52832",
        },
        "bbc_microbit": {
            "name": "micro:bit",
            "variant": "NRF51822",
        },
        "bbc_microbit_v2": {
            "name": "micro:bit V2",
            "variant": "NRF52833",
        },
        "bl54l15_dvk/nrf54l10/cpuapp": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L10",
        },
        "bl54l15_dvk/nrf54l10/cpuapp/ns": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L10",
        },
        "bl54l15_dvk/nrf54l15/cpuapp": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15_dvk/nrf54l15/cpuapp/ns": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15_dvk/nrf54l15/cpuflpr": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15_dvk/nrf54l15/cpuflpr/xip": {
            "name": "BL54L15 DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuapp": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuapp/ns": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuflpr": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl54l15u_dvk/nrf54l15/cpuflpr/xip": {
            "name": "BL54L15u DVK",
            "variant": "NRF54L15",
        },
        "bl652_dvk": {
            "name": "BL652 DVK",
            "variant": "NRF52832",
        },
        "bl653_dvk": {
            "name": "BL653 DVK",
            "variant": "NRF52833",
        },
        "bl654_dvk": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
        },
        "bl654_dvk/nrf52840/pa": {
            "name": "BL654 DVK",
            "variant": "NRF52840",
        },
        "bl654_sensor_board": {
            "name": "BL654 Sensor Board",
            "variant": "NRF52840",
        },
        "bl654_usb": {
            "name": "BL654 USB (451-00004)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "bl654_usb/nrf52840/bare": {
            "name": "BL654 USB (451-00004)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "blueclover_plt_demo_v2": {
            "name": "Blue Clover PLT Demo V2 nRF52832",
            "variant": "NRF52832",
        },
        "bt510": {
            "name": "Sentrius BT510 Sensor",
            "variant": "NRF52840",
        },
        "bt610": {
            "name": "Sentrius BT610 Sensor",
            "variant": "NRF52840",
        },
        "bytesensi_l": {
            "name": "byteSENSI-L",
            "variant": "NRF52832",
        },
        "contextualelectronics_abc": {
            "name": "Advanced BLE Cell",
            "variant": "NRF52840",
        },
        "croxel_cx1825": {
            "name": "CX1825 nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "ctcc/nrf52840": {
            "name": "CTHINGS.CO Connectivity Card",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "decawave_dwm1001_dev": {
            "name": "Decawave DWM1001",
            "variant": "NRF52832",
        },
        "decawave_dwm3001cdk": {
            "name": "Decawave DWM3001CDK",
            "variant": "NRF52833",
        },
        "degu_evk": {
            "name": "Degu Evaluation Kit",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "ebyte_e73_tbb": {
            "name": "E73-TBB",
            "variant": "NRF52832",
        },
        "holyiot_21014": {
            "name": "21014 Module",
            "variant": "NRF52810",
        },
        "holyiot_25008/nrf54l15/cpuapp": {
            "name": "HOLYIOT 25008",
            "variant": "NRF54L15",
        },
        "holyiot_yj16019": {
            "name": "YJ-16019",
            "variant": "NRF52832",
        },
        "holyiot_yj17095": {
            "name": "YJ-17095",
            "variant": "NRF52832",
        },
        "mg100": {
            "name": "Sentrius™ MG100 Gateway",
            "variant": "NRF52840",
        },
        "micromod": {
            "name": "MicroMod board Processor",
            "variant": "NRF52840",
        },
        "nrf21540dk": {
            "name": "nRF21540 DK",
            "variant": "NRF52840",
        },
        "nrf51_ble400": {
            "name": "BLE400",
            "variant": "NRF51822",
        },
        "nrf51_blenano": {
            "name": "Redbear Labs Nano",
            "variant": "NRF51822",
        },
        "nrf51_vbluno51": {
            "name": "nRF51-VBLUno51",
            "variant": "NRF51822",
        },
        "nrf51dk": {
            "name": "nRF51 DK",
            "variant": "NRF51822",
        },
        "nrf51dongle": {
            "name": "nRF51 Dongle",
            "variant": "NRF51822",
        },
        "nrf52820dongle": {
            "name": "nRF52820 Dongle",
            "variant": "NRF52820",
        },
        "nrf52832_mdk": {
            "name": "nRF52832-mdk",
            "variant": "NRF52832",
        },
        "nrf52833dk/nrf52820": {
            "name": "nRF52833 DK",
            "variant": "NRF52820",
        },
        "nrf52833dk/nrf52833": {
            "name": "nRF52833 DK",
            "variant": "NRF52833",
        },
        "nrf52833dongle": {
            "name": "nRF52833 Dongle",
            "variant": "NRF52833",
        },
        "nrf52840_blip": {
            "name": "Labs Blip",
            "variant": "NRF52840",
        },
        "nrf52840_mdk": {
            "name": "nRF52840-mdk",
            "variant": "NRF52840",
        },
        "nrf52840_mdk_usb_dongle": {
            "name": "nRF52840 MDK USB Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "nrf52840_papyr": {
            "name": "Labs Papyr",
            "variant": "NRF52840",
        },
        "nrf52840dk/nrf52811": {
            "name": "nRF52840 DK",
            "variant": "NRF52811",
        },
        "nrf52840dk/nrf52840": {
            "name": "nRF52840 DK",
            "variant": "NRF52840",
        },
        "nrf52840dongle": {
            "name": "nRF52840 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "nrf52840dongle/nrf52840/bare": {
            "name": "nRF52840 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "nrf52840gmouse": {
            "name": "nRF52840 Gaming Mouse",
            "variant": "NRF52840",
        },
        "nrf52_adafruit_feather": {
            "name": "nRF52 Adafruit Feather",
            "variant": "NRF52832",
        },
        "nrf52_blenano2": {
            "name": "Redbear Labs Nano v2",
            "variant": "NRF52832",
        },
        "nrf52_sparkfun": {
            "name": "nRF52832 breakout",
            "variant": "NRF52832",
        },
        "nrf52_vbluno52": {
            "name": "nRF52-VBLUno52",
            "variant": "NRF52832",
        },
        "nrf52dk/nrf52805": {
            "name": "nRF52 DK",
            "variant": "NRF52805",
        },
        "nrf52dk/nrf52810": {
            "name": "nRF52 DK",
            "variant": "NRF52810",
        },
        "nrf52dk/nrf52832": {
            "name": "nRF52 DK",
            "variant": "NRF52832",
        },
        "nrf52dmouse": {
            "name": "nRF52 Desktop Mouse",
            "variant": "NRF52832",
        },
        "nrf52kbd": {
            "name": "nRF52 Keyboard",
            "variant": "NRF52832",
        },
        "nrf54h20dk/nrf54h20/cpuapp": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuflpr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuflpr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuppr": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpuppr/xip": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54h20dk/nrf54h20/cpurad": {
            "name": "nRF54H20 DK",
            "variant": "NRF54H20",
        },
        "nrf54l15bsim/nrf54l15/cpuapp": {
            "name": "nRF54L15 simulated boards (BabbleSim)",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l05/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L05",
        },
        "nrf54l15dk/nrf54l05/cpuflpr": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L05",
        },
        "nrf54l15dk/nrf54l10/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
        },
        "nrf54l15dk/nrf54l10/cpuapp/ns": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
        },
        "nrf54l15dk/nrf54l10/cpuflpr": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L10",
        },
        "nrf54l15dk/nrf54l15/cpuapp": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuapp/ns": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuflpr": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15dk/nrf54l15/cpuflpr/xip": {
            "name": "nRF54L15 DK",
            "variant": "NRF54L15",
        },
        "nrf54l15tag/nrf54l15/cpuapp": {
            "name": "nRF54L15 TAG",
            "variant": "NRF54L15",
        },
        "nrf54l15tag/nrf54l15/cpuapp/ns": {
            "name": "nRF54L15 TAG",
            "variant": "NRF54L15",
        },
        "nrf54l15tag/nrf54l15/cpuflpr": {
            "name": "nRF54L15 TAG",
            "variant": "NRF54L15",
        },
        "nrf54l15tag/nrf54l15/cpuflpr/xip": {
            "name": "nRF54L15 TAG",
            "variant": "NRF54L15",
        },
        "nrf54lc10dk/nrf54lc10a/cpuapp": {
            "name": "nRF54LC10 DK",
            "variant": "NRF54LC10A",
        },
        "nrf54lc10dk/nrf54lc10a/cpuapp/ns": {
            "name": "nRF54LC10 DK",
            "variant": "NRF54LC10A",
        },
        "nrf54lc10dk/nrf54lc10a/cpuflpr": {
            "name": "nRF54LC10 DK",
            "variant": "NRF54LC10A",
        },
        "nrf54lm20bsim/nrf54lm20a/cpuapp": {
            "name": "nRF54LM20 simulated boards (BabbleSim)",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20a/cpuapp": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20a/cpuapp/ns": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20a/cpuflpr": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20A",
        },
        "nrf54lm20dk/nrf54lm20b/cpuapp": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20B",
        },
        "nrf54lm20dk/nrf54lm20b/cpuapp/ns": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20B",
        },
        "nrf54lm20dk/nrf54lm20b/cpuflpr": {
            "name": "nRF54LM20 DK",
            "variant": "NRF54LM20B",
        },
        "nrf54lm20dongle/nrf54lm20b/cpuapp": {
            "name": "nRF54LM20 Dongle",
            "variant": "NRF54LM20B",
            "cdc_acm": [
                "/soc/peripheral@50000000/usbhs_wrapper@5a000/usbhs@20000/board_cdc_acm_uart",
                "/soc/peripheral@50000000/usbhs_wrapper@5a000/usbhs@20000/mcumgr_cdc_acm_uart",
            ],
        },
        "nrf54lm20dongle/nrf54lm20b/cpuapp/ns": {
            "name": "nRF54LM20 Dongle",
            "variant": "NRF54LM20B",
            "cdc_acm": [
                "/soc/peripheral@40000000/usbhs_wrapper@5a000/usbhs@20000/board_cdc_acm_uart",
                "/soc/peripheral@40000000/usbhs_wrapper@5a000/usbhs@20000/mcumgr_cdc_acm_uart",
            ],
        },
        "nrf54lm20dongle/nrf54lm20b/cpuflpr": {
            "name": "nRF54LM20 Dongle",
            "variant": "NRF54LM20B",
        },
        "nrf54ls05dk/nrf54ls05a/cpuapp": {
            "name": "nRF54LS05B DK",
            "variant": "NRF54LS05A",
        },
        "nrf54ls05dk/nrf54ls05b/cpuapp": {
            "name": "nRF54LS05B DK",
            "variant": "NRF54LS05B",
        },
        "nrf54lv10dk/nrf54lv10a/cpuapp": {
            "name": "nRF54LV10 DK",
            "variant": "NRF54LV10A",
        },
        "nrf54lv10dk/nrf54lv10a/cpuapp/ns": {
            "name": "nRF54LV10 DK",
            "variant": "NRF54LV10A",
        },
        "nrf54lv10dk/nrf54lv10a/cpuflpr": {
            "name": "nRF54LV10 DK",
            "variant": "NRF54LV10A",
        },
        "nrf9160dk/nrf52840": {
            "name": "nRF9160 DK",
            "variant": "NRF52840",
        },
        "ophelia4ev/nrf54l15/cpuapp": {
            "name": "Ophelia-IV DK",
            "variant": "NRF54L15",
        },
        "ophelia4ev/nrf54l15/cpuflpr": {
            "name": "Ophelia-IV DK",
            "variant": "NRF54L15",
        },
        "p2d": {
            "name": "Pebble 2 Duo",
            "variant": "NRF52840",
        },
        "pan1770_evb": {
            "name": "PAN1770 Evaluation Board",
            "variant": "NRF52840",
        },
        "pan1780_evb": {
            "name": "PAN1780 Evaluation Board",
            "variant": "NRF52840",
        },
        "pan1781_evb": {
            "name": "PAN1781 Evaluation Board",
            "variant": "NRF52820",
        },
        "pan1782_evb": {
            "name": "PAN1782 Evaluation Board",
            "variant": "NRF52833",
        },
        "panb611evb/nrf54l15/cpuapp": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "panb611evb/nrf54l15/cpuapp/ns": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "panb611evb/nrf54l15/cpuflpr": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "panb611evb/nrf54l15/cpuflpr/xip": {
            "name": "PAN B611 Evaluation Board",
            "variant": "NRF54L15",
        },
        "particle_argon": {
            "name": "Argon",
            "variant": "NRF52840",
        },
        "particle_boron": {
            "name": "Boron",
            "variant": "NRF52840",
        },
        "particle_xenon": {
            "name": "Xenon",
            "variant": "NRF52840",
        },
        "pinetime_devkit0": {
            "name": "PineTime DevKit0",
            "variant": "NRF52832",
        },
        "pinnacle_100_dvk": {
            "name": "Pinnacle 100 DVK",
            "variant": "NRF52840",
        },
        "promicro_nrf52840": {
            "name": "Pro Micro nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "promicro_nrf52840/nrf52840/uf2": {
            "name": "Pro Micro nRF52840",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "qemu_cortex_m0": {
            "name": "QEMU Emulation for ARM Cortex-M0",
            "variant": "NRF51822",
        },
        "quill_nrf52840_mesh": {
            "name": "Quill nRF52840 Mesh",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "rak4631": {
            "name": "RAK4631",
            "variant": "NRF52840",
        },
        "rak5010": {
            "name": "RAK5010",
            "variant": "NRF52840",
        },
        "raytac_an54lq_db_15/nrf54l15/cpuapp": {
            "name": "AN54LQ-DB-15",
            "variant": "NRF54L15",
        },
        "raytac_an54lq_db_15/nrf54l15/cpuapp/ns": {
            "name": "AN54LQ-DB-15",
            "variant": "NRF54L15",
        },
        "raytac_an54lq_db_15/nrf54l15/cpuflpr": {
            "name": "AN54LQ-DB-15",
            "variant": "NRF54L15",
        },
        "raytac_an54lq_db_15/nrf54l15/cpuflpr/xip": {
            "name": "AN54LQ-DB-15",
            "variant": "NRF54L15",
        },
        "raytac_mdbt50q_cx_40_dongle": {
            "name": "MDBT50Q-CX-40 Dongle",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "raytac_mdbt50q_db_33": {
            "name": "MDBT50Q-DB-33",
            "variant": "NRF52833",
        },
        "raytac_mdbt50q_db_40": {
            "name": "MDBT50Q-DB-40",
            "variant": "NRF52840",
        },
        "reel_board": {
            "name": "reel board",
            "variant": "NRF52840",
        },
        "rm1xx_dvk": {
            "name": "RM1xx DVK",
            "variant": "NRF51822",
        },
        "ruuvi_ruuvitag": {
            "name": "RuuviTag",
            "variant": "NRF52832",
        },
        "thingy52": {
            "name": "Thingy:52",
            "variant": "NRF52832",
        },
        "thingy91/nrf52840": {
            "name": "Thingy:91",
            "variant": "NRF52840",
        },
        "ubx_bmd300eval": {
            "name": "EVK-BMD-30/35: BMD-300-EVAL, BMD-301-EVAL, and BMD-350-EVAL",
            "variant": "NRF52832",
        },
        "ubx_bmd330eval": {
            "name": "EVK-BMD-330: BMD-330-EVAL",
            "variant": "NRF52810",
        },
        "ubx_bmd340eval": {
            "name": "EVK-BMD-34/38: BMD-340-EVAL and BMD-341-EVAL",
            "variant": "NRF52840",
        },
        "ubx_bmd345eval": {
            "name": "EVK-BMD-34/38: BMD-345-EVAL",
            "variant": "NRF52840",
        },
        "ubx_bmd360eval": {
            "name": "EVK-BMD-360: BMD-360-EVAL",
            "variant": "NRF52811",
        },
        "ubx_bmd380eval": {
            "name": "EVK-BMD-34/48: BMD-380-EVAL",
            "variant": "NRF52840",
        },
        "ubx_evkannab1": {
            "name": "EVK-ANNA-B11x",
            "variant": "NRF52832",
        },
        "ubx_evkninab1": {
            "name": "EVK NINA-B11x",
            "variant": "NRF52832",
        },
        "ubx_evkninab3": {
            "name": "EVK-NINA-B3",
            "variant": "NRF52840",
        },
        "ubx_evkninab4": {
            "name": "EVK NINA-B40x",
            "variant": "NRF52833",
        },
        "ubx_evknorab2/nrf54l05/cpuapp": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L05",
        },
        "ubx_evknorab2/nrf54l05/cpuflpr": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L05",
        },
        "ubx_evknorab2/nrf54l10/cpuapp": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L10",
        },
        "ubx_evknorab2/nrf54l10/cpuapp/ns": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L10",
        },
        "ubx_evknorab2/nrf54l10/cpuflpr": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L10",
        },
        "ubx_evknorab2/nrf54l15/cpuapp": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L15",
        },
        "ubx_evknorab2/nrf54l15/cpuapp/ns": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L15",
        },
        "ubx_evknorab2/nrf54l15/cpuflpr": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L15",
        },
        "ubx_evknorab2/nrf54l15/cpuflpr/xip": {
            "name": "EVK-NORA-B2",
            "variant": "NRF54L15",
        },
        "we_ophelia1ev": {
            "name": "Ophelia-I EV NRF52805",
            "variant": "NRF52805",
        },
        "we_proteus2ev": {
            "name": "Proteus-II-EV",
            "variant": "NRF52832",
        },
        "we_proteus3ev": {
            "name": "Proteus-III-EV",
            "variant": "NRF52840",
        },
        "wio_wm1110_dev_kit": {
            "name": "Wio-WM1110 Dev Kit",
            "variant": "NRF52840",
        },
        "xiao_ble": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "xiao_ble/nrf52840/sense": {
            "name": "XIAO BLE (Sense)",
            "variant": "NRF52840",
            "cdc_acm": [
                "/soc/usbd@40027000/board_cdc_acm_uart",
            ],
        },
        "xiao_nrf54l15/nrf54l15/cpuapp": {
            "name": "XIAO NRF54L15",
            "variant": "NRF54L15",
        },
        "xiao_nrf54l15/nrf54l15/cpuflpr": {
            "name": "XIAO NRF54L15",
            "variant": "NRF54L15",
        },
    },
}
# DO NOT ADD ANYTHING BELOW THIS LINE
