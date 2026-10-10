from esphome.components.zephyr import Section
from esphome.components.zephyr.const import KEY_BOOTLOADER

from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
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
    # Nordic nRF52840 Dongle (PCA10059): factory Open DFU bootloader at 0xE0000, kept in place
    "nrf52840dongle": {KEY_BOOTLOADER: [BOOTLOADER_NRF]},
}

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
    # The partition manager places the application and settings between these
    BOOTLOADER_NRF: [
        Section("mbr", 0x0, 0x1000, "flash_primary"),
        Section("open_bootloader", 0xE0000, 0x20000, "flash_primary"),
    ],
}
