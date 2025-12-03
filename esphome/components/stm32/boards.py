import json
import re

from esphome import platformio_api
import esphome.config_validation as cv
from esphome.const import CONF_BOARD

from .const import KEY_FCPU, KEY_MCU, KEY_MCU_SERIES, KEY_RAM, KEY_ROM

STM32_BASE_PINS = {
    "LED": 5,
}

STM32_BOARD_PINS = {}

MCU_RE = re.compile("stm32([fghlu][0-9])(.*)", re.IGNORECASE)


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


def get_board_details(platform):
    board = platform[CONF_BOARD]
    board_details = platformio_get_board_details(board)

    details = {k: board_details[k] for k in (KEY_FCPU, KEY_MCU, KEY_RAM, KEY_ROM)}

    if match := MCU_RE.match(details[KEY_MCU]):
        mcu_series, _ = match.groups()
    else:
        raise cv.Invalid(f"Can't detect board series for '{board}'")

    details.update(
        {
            KEY_MCU_SERIES: mcu_series,
        }
    )

    return details
