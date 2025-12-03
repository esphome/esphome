import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_PLATFORM,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_STM32,
    ThreadModel,
)
from esphome.core import CORE, coroutine_with_priority

from .boards import validate_board_details
from .const import (
    CONF_FCPU,
    CONF_MCU,
    CONF_MCU_SERIES,
    CONF_RAM,
    CONF_ROM,
    KEY_FCPU,
    KEY_GPIO_CLOCK_ENABLED,
    KEY_MCU,
    KEY_MCU_SERIES,
    KEY_RAM,
    KEY_ROM,
    KEY_STM32,
    KEY_UART_INSTANCES,
)
from .gpio import stm32_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@mrk-its"]
AUTO_LOAD = []
IS_TARGET_PLATFORM = True


def set_core_data(config):
    stm32_data = CORE.data[KEY_STM32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_STM32
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "stm32cube"

    # TODO
    stm32_data[KEY_UART_INSTANCES] = [
        "USART1",
        "USART2",
        "USART3",
        "UART4",
        "UART5",
        "LPUART1",
    ]
    stm32_data[KEY_GPIO_CLOCK_ENABLED] = set()
    stm32_data[KEY_FCPU] = config[CONF_FCPU]
    stm32_data[KEY_MCU] = config[CONF_MCU]
    stm32_data[KEY_MCU_SERIES] = config[CONF_MCU_SERIES]
    stm32_data[KEY_RAM] = config[CONF_RAM]
    stm32_data[KEY_ROM] = config[CONF_ROM]

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict,
            ),
            cv.Optional(CONF_PLATFORM, default="ststm32"): cv.string_strict,
            cv.Optional(CONF_MCU): cv.string_strict,
            cv.Optional(CONF_MCU_SERIES): cv.string_strict,
            cv.Optional(CONF_FCPU): cv.int_,
            cv.Optional(CONF_ROM): cv.int_,
            cv.Optional(CONF_RAM): cv.int_,
        }
    ),
    validate_board_details,
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_STM32")
    cg.add_platformio_option(
        "platform_packages", "toolchain-gccarmnoneeabi @ ~1.120301.0"
    )
    cg.set_cpp_standard("gnu++20")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32")
    cg.add_define(ThreadModel.SINGLE)

    cg.add_platformio_option("framework", "stm32cube")
    cg.add_platformio_option("platform", config[CONF_PLATFORM])
    cg.add_platformio_option("monitor_speed", "115200")
    cg.add_platformio_option("upload_protocol", "stlink")
