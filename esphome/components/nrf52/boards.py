from esphome.components.zephyr import ZephyrBoard
from esphome.const import CONF_ID, CONF_NAME, CONF_TYPE

from .boards_generated import BOARDS as _GENERATED_BOARDS
from .const import (
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NORDIC,
    CONF_BOOTLOADER,
    CONF_EXT_FLASH,
    CONF_FLASH_SIZE,
    CONF_MCU,
    CONF_PARTITIONS,
    CONF_RAM_SIZE,
    CONF_VENDOR,
    MCU_NRF54H20,
    MCU_NRF54L05,
    MCU_NRF54L10,
    MCU_NRF54L15,
    MCU_NRF54L20,
    MCU_NRF5340,
    MCU_NRF52810,
    MCU_NRF52811,
    MCU_NRF52820,
    MCU_NRF52832,
    MCU_NRF52833,
    MCU_NRF52840,
)


def _SIZE_MB(flash_mb: int) -> int:
    return flash_mb * 1024 * 1024


def _SIZE_KB(flash_kb: int) -> int:
    return flash_kb * 1024


FLASH_SIZE_MAP = {
    MCU_NRF54H20: _SIZE_MB(2),
    MCU_NRF54L20: _SIZE_MB(2),
    MCU_NRF54L15: _SIZE_KB(1536),  # 1.5 MB
    MCU_NRF54L10: _SIZE_MB(1),
    MCU_NRF54L05: _SIZE_KB(512),
    MCU_NRF5340: _SIZE_MB(1),
    MCU_NRF52840: _SIZE_MB(1),
    MCU_NRF52833: _SIZE_KB(512),
    MCU_NRF52832: _SIZE_KB(
        512
    ),  # This can be 256KB on some versions of that chip. Needs to be customized
    MCU_NRF52820: _SIZE_KB(256),
    MCU_NRF52811: _SIZE_KB(192),
    MCU_NRF52810: _SIZE_KB(192),
}

RAM_SIZE_MAP = {
    MCU_NRF54H20: _SIZE_MB(1),
    MCU_NRF54L20: _SIZE_KB(512),
    MCU_NRF54L15: _SIZE_KB(256),  # 1.5 MB
    MCU_NRF54L10: _SIZE_KB(192),
    MCU_NRF54L05: _SIZE_KB(96),
    MCU_NRF5340: _SIZE_KB(512),
    MCU_NRF52840: _SIZE_KB(256),
    MCU_NRF52833: _SIZE_KB(128),
    MCU_NRF52832: _SIZE_KB(
        64
    ),  # This can be 32KB on some versions of that chip. Needs to be customized
    MCU_NRF52820: _SIZE_KB(32),
    MCU_NRF52811: _SIZE_KB(24),
    MCU_NRF52810: _SIZE_KB(25),
}

BOOTLOADER_PARTITIONS_NORDIC = [
    {"name": "nrf5_mbr", "address": 0x0, "size": 0x1000},
    {"name": "nrf5_bootloader", "address": 0xE0000, "size": 0x20000},
]

BOOTLOADER_PARTITIONS_NRF52_SD140_V6 = [
    {"name": "softdevice", "address": 0x0, "size": 0x26000},
    {"name": "adafruit_bootloader", "address": 0xF4000, "size": 0xC000},
]

BOOTLOADER_PARTITIONS_NRF52_SD140_V7 = [
    {"name": "softdevice", "address": 0x0, "size": 0x27000},
    {"name": "adafruit_bootloader", "address": 0xF4000, "size": 0xC000},
]

BOOTLOADER_PARTITION_MAP = {
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: BOOTLOADER_PARTITIONS_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: BOOTLOADER_PARTITIONS_NRF52_SD140_V7,
    BOOTLOADER_NORDIC: BOOTLOADER_PARTITIONS_NORDIC,
}

# Customizations to generated boards
# Those are not set correctly in the generated data
BOARD_OVERRIDES = {
    "nrf52840dongle": {
        CONF_BOOTLOADER: {
            CONF_TYPE: BOOTLOADER_NORDIC,
            CONF_PARTITIONS: BOOTLOADER_PARTITIONS_NORDIC,
        },
    },
    "adafruit_itsybitsy_nrf52840": _GENERATED_BOARDS[
        "adafruit_itsybitsy"
    ],  # sdk v2.6.1 compatibility
}

# Merge generated boards with overrides
BOARDS = {
    board_name: (_GENERATED_BOARDS.get(board_name, {}) | overrides)
    for board_name, overrides in BOARD_OVERRIDES.items()
} | {
    board_name: board_info
    for board_name, board_info in _GENERATED_BOARDS.items()
    if board_name not in BOARD_OVERRIDES
}


class Nrf52Board(ZephyrBoard):
    @classmethod
    def from_dict(cls, board_info: dict) -> "Nrf52Board":
        return cls(
            id=board_info[CONF_ID],
            name=board_info[CONF_NAME],
            mcu=board_info[CONF_MCU],
            vendor=board_info[CONF_VENDOR],
            flash_size=board_info[CONF_FLASH_SIZE],
            ram_size=(
                board_info[CONF_RAM_SIZE]
                if CONF_RAM_SIZE in board_info
                else RAM_SIZE_MAP[board_info[CONF_MCU]]
            ),
            external_flash=board_info[CONF_EXT_FLASH],
            bootloader=board_info.get(CONF_BOOTLOADER),
        )
