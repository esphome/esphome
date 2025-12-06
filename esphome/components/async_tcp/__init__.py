# Async TCP client support for all platforms
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]


def AUTO_LOAD() -> list[str]:
    # Socket component only needed for non-Arduino platforms
    if not CORE.using_arduino:
        return ["socket"]
    return []


# Support all platforms - Arduino gets library, ESP-IDF/host get socket implementation
CONFIG_SCHEMA = cv.Schema({})


@coroutine_with_priority(CoroPriority.NETWORK_TRANSPORT)
async def to_code(config):
    if CORE.using_arduino:
        if CORE.is_esp32 or CORE.is_libretiny:
            # https://github.com/ESP32Async/AsyncTCP
            cg.add_library("ESP32Async/AsyncTCP", "3.4.5")
        elif CORE.is_esp8266:
            # https://github.com/ESP32Async/ESPAsyncTCP
            cg.add_library("ESP32Async/ESPAsyncTCP", "2.0.0")
    # ESP-IDF and host use socket-based implementation (no library needed)


def FILTER_SOURCE_FILES() -> list[str]:
    # Only compile socket implementation for non-Arduino platforms
    if CORE.using_arduino:
        return ["async_tcp_socket.cpp"]
    return []
