from esphome.components.zephyr import Section
from esphome.components.zephyr.const import KEY_BOARD, KEY_BOOTLOADER

from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V2,
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
    CONF_DCDC,
)

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
    # Adafruit Feather nRF52832: current bootloader with S132 6.1.1, or the original 0.5.0 with S132 2.0.1.
    # Zephyr has no board for it, so it builds on the nRF52 DK, whose console UART uses the same pins;
    # the config keeps the ESPHome name. Its bootloader leaves the regulator in LDO mode, while the DK
    # turns DCDC on
    "adafruit_feather_nrf52832": {
        KEY_BOOTLOADER: [
            BOOTLOADER_ADAFRUIT_NRF52_SD132_V6,
            BOOTLOADER_ADAFRUIT_NRF52_SD132_V2,
        ],
        KEY_BOARD: "nrf52dk/nrf52832",
        CONF_DCDC: False,
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
    # nRF52832, dual bank 0.5.0: app at most 0x28800 at 0x1C000, second bank above it; settings in the
    # app data pages it keeps
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V2: [
        Section("SoftDevice", 0x0, 0x1C000, "flash_primary"),
        Section("Adafruit_nRF52_Bootloader_bank1", 0x44800, 0x2D800, "flash_primary"),
        Section("settings_storage", 0x72000, 0x2000, "flash_primary"),
        Section("Adafruit_nRF52_Bootloader", 0x74000, 0xC000, "flash_primary"),
    ],
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V6: [
        Section("SoftDevice", 0x0, 0x26000, "flash_primary"),
        Section("Adafruit_nRF52_Bootloader", 0x74000, 0xC000, "flash_primary"),
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

# Zephyr needs the SoC and CPU cluster for boards that have several; a short name picks the usual one
BOARD_ALIASES = {
    "nrf54lm20dk": "nrf54lm20dk/nrf54lm20a/cpuapp",
    "nrf54l15dk": "nrf54l15dk/nrf54l15/cpuapp",
}
