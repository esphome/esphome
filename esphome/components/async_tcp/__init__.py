# Dummy integration to allow relying on AsyncTCP
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@OttoWinter"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_arduino,
    cv.only_on(["esp32", "esp8266"]),
)


@coroutine_with_priority(200.0)
async def to_code(config):
    if CORE.is_esp32:
        cg.add_library(
            None,
            None,
            "https://github.com/yubox-node-org/AsyncTCPSock#1212289f0b9bdf81adda4846043be5ff3361070e",
        )
        cg.add_platformio_option("lib_ignore", ["AsyncTCP", "AsyncTCP-esphome"])
    elif CORE.is_esp8266:
        # https://github.com/esphome/ESPAsyncTCP
        cg.add_library("esphome/ESPAsyncTCP-esphome", "1.2.3")
