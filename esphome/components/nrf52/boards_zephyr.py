from .const import BOOTLOADER_ADAFRUIT
from esphome.components.zephyr.const import KEY_BOOTLOADER

BOARDS_ZEPHYR = {
    "adafruit_itsybitsy_nrf52840": {KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT},
}
