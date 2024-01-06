# Dummy integration to allow relying on AsyncTCP
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority
from esphome.const import (
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
    PLATFORM_RTL87XX,
)

CODEOWNERS = ["@OttoWinter"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_arduino,
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX, PLATFORM_RTL87XX]),
)


@coroutine_with_priority(200.0)
async def to_code(config):
    if CORE.is_esp32 or CORE.is_libretiny:
        # https://github.com/esphome/AsyncTCP/blob/master/library.json
        cg.add_library("esphome/AsyncTCP-esphome", "2.0.1")
    elif CORE.is_esp8266:
        # https://github.com/esphome/ESPAsyncTCP
        cg.add_library("esphome/ESPAsyncTCP-esphome", "2.0.0")
