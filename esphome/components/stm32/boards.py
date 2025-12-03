import json
import re

from esphome import platformio_api
import esphome.config_validation as cv
from esphome.const import CONF_BOARD

from .const import CONF_FCPU, CONF_MCU, CONF_MCU_SERIES, CONF_RAM, CONF_ROM

STM32_BASE_PINS = {
    "LED": 5,
}

STM32_BOARD_PINS = {}

MCU_RE = re.compile("STM32([FGHLU][0-9])(.*)", re.IGNORECASE)


def platformio_get_board_details(board):
    boards = json.loads(
        platformio_api.run_platformio_cli(
            "boards", "--json-output", board, capture_stdout=True
        )
    )
    boards_data = [b for b in boards if b["id"] == board]
    if len(boards_data) != 1:
        raise cv.Invalid(f"Unknown board: '{board}'")
    return boards_data[0]


def validate_board_details(platform):
    def set_if_empty(key, value_factory):
        if not platform.get(key):
            platform[key] = value_factory()

    board = platform[CONF_BOARD]
    board_details = platformio_get_board_details(board)

    set_if_empty(CONF_MCU, lambda: board_details["mcu"])
    match = MCU_RE.match(platform[CONF_MCU])
    if not match:
        raise cv.Invalid(f"invalid MCU: {platform[CONF_MCU]}")
    platform[CONF_MCU] = platform[CONF_MCU].upper()
    set_if_empty(CONF_MCU_SERIES, lambda: match.group(1).upper())
    set_if_empty(CONF_FCPU, lambda: board_details["fcpu"])
    set_if_empty(CONF_RAM, lambda: board_details["ram"])
    set_if_empty(CONF_ROM, lambda: board_details["rom"])

    return platform
