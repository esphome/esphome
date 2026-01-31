# Async TCP client support for all platforms
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]


def AUTO_LOAD() -> list[str]:
    # Socket component needed for platforms using socket-based implementation
    # ESP32, ESP8266, RP2040, and LibreTiny use AsyncTCP libraries, others use sockets
    if (
        not CORE.is_esp32
        and not CORE.is_esp8266
        and not CORE.is_rp2040
        and not CORE.is_libretiny
    ):
        return ["socket"]
    return []


# Support all platforms - Arduino/ESP-IDF get libraries, other platforms use socket implementation
CONFIG_SCHEMA = cv.Schema({})


@coroutine_with_priority(CoroPriority.NETWORK_TRANSPORT)
async def to_code(config):
    if CORE.is_esp32:
        # https://github.com/ESP32Async/AsyncTCP
        from esphome.components.esp32 import add_idf_component

        add_idf_component(name="esp32async/asynctcp", ref="3.4.91")
    elif CORE.is_libretiny:
        # https://github.com/ESP32Async/AsyncTCP
        cg.add_library("ESP32Async/AsyncTCP", "3.4.5")
    elif CORE.is_esp8266:
        # https://github.com/ESP32Async/ESPAsyncTCP
        cg.add_library("ESP32Async/ESPAsyncTCP", "2.0.0")
    elif CORE.is_rp2040:
        # https://github.com/khoih-prog/AsyncTCP_RP2040W
        cg.add_library("khoih-prog/AsyncTCP_RP2040W", "1.2.0")
    # Other platforms (host, etc) use socket-based implementation


def FILTER_SOURCE_FILES() -> list[str]:
    # Exclude socket implementation for platforms that use AsyncTCP libraries
    if CORE.is_esp32 or CORE.is_esp8266 or CORE.is_rp2040 or CORE.is_libretiny:
        return ["async_tcp_socket.cpp"]
    return []
