import json
import re

from esphome import platformio_api
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, CONF_PLATFORM

from .const import CONF_FCPU, CONF_MCU, CONF_MCU_SERIES, CONF_RAM, CONF_ROM

STM32_BASE_PINS = {
    "LED": 5,
}

STM32_BOARD_PINS = {}

MCU_RE = re.compile("STM32([FGHLU][0-9])(.*)", re.IGNORECASE)


def platformio_get_board_details(platform, board):
    if platform.startswith("https://"):
        platformio_api.run_platformio_cli("pkg", "install", "-g", "-p", platform)
    boards = json.loads(
        platformio_api.run_platformio_cli(
            "boards", "--json-output", board, capture_stdout=True
        )
    )
    boards_data = [b for b in boards if b["id"] == board]
    if len(boards_data) != 1:
        raise cv.Invalid(f"Unknown board: '{board}'")
    return boards_data[0]


def validate_board_details(config):
    def set_if_empty(key, value_factory):
        if not config.get(key):
            config[key] = value_factory()

    board = config[CONF_BOARD]
    platform = config[CONF_PLATFORM]
    board_details = platformio_get_board_details(platform, board)

    set_if_empty(CONF_MCU, lambda: board_details["mcu"])
    match = MCU_RE.match(config[CONF_MCU])
    if not match:
        raise cv.Invalid(f"invalid MCU: {config[CONF_MCU]}")
    config[CONF_MCU] = config[CONF_MCU].upper()
    set_if_empty(CONF_MCU_SERIES, lambda: match.group(1).upper())
    set_if_empty(CONF_FCPU, lambda: board_details["fcpu"])
    set_if_empty(CONF_RAM, lambda: board_details["ram"])
    set_if_empty(CONF_ROM, lambda: board_details["rom"])

    return config
