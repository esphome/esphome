from esphome.components.zephyr.const import KEY_BOOTLOADER
from .const import BOOTLOADER_ADAFRUIT

BOARDS_ZEPHYR = {
    "adafruit_itsybitsy_nrf52840": {KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT},
}
