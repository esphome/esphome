# Dummy integration to allow relying on AsyncTCP
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@OttoWinter"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_arduino,
    cv.only_on(["esp32", "esp8266", "bk72xx", "rtl87xx"]),
)


@coroutine_with_priority(200.0)
async def to_code(config):
    if CORE.is_esp32 or CORE.is_libretiny:
        # https://github.com/esphome/AsyncTCP/blob/master/library.json
        cg.add_library("esphome/AsyncTCP-esphome", "2.0.1")
    elif CORE.is_esp8266:
        # https://github.com/esphome/ESPAsyncTCP
        cg.add_library("esphome/ESPAsyncTCP-esphome", "2.0.0")
