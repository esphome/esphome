# Async TCP client support for all platforms
import esphome.codegen as cg
from esphome.components.libretiny.const import CONF_LOGLEVEL
import esphome.config_validation as cv
from esphome.const import CONF_FRAMEWORK, CONF_OPTIONS
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]
DOMAIN = "async_tcp"

# LT_LOGLEVEL values that compile LibreTiny's log_d in (lt_config.h: VERBOSE is TRACE 0, DEBUG 1)
_LT_DEBUG_LOGLEVELS = {"LT_LEVEL_VERBOSE", "LT_LEVEL_TRACE", "LT_LEVEL_DEBUG", "0", "1"}


def _libretiny_logs_debug() -> bool:
    """Whether LibreTiny compiles log_d in; framework options override loglevel."""
    framework = CORE.config[CORE.target_platform][CONF_FRAMEWORK]
    level = framework[CONF_OPTIONS].get(
        "LT_LOGLEVEL", f"LT_LEVEL_{framework[CONF_LOGLEVEL]}"
    )
    return level.strip() in _LT_DEBUG_LOGLEVELS


def AUTO_LOAD() -> list[str]:
    # Socket component needed for platforms using socket-based implementation
    # ESP32, ESP8266, RP2040, and LibreTiny use AsyncTCP libraries, others use sockets
    if (
        not CORE.is_esp32
        and not CORE.is_esp8266
        and not CORE.is_rp2
        and not CORE.is_libretiny
    ):
        return ["socket"]
    return []


# Support all platforms - Arduino/ESP-IDF get libraries, other platforms use socket implementation
CONFIG_SCHEMA = cv.Schema({})


@coroutine_with_priority(CoroPriority.NETWORK_TRANSPORT)
async def to_code(config: ConfigType) -> None:
    if CORE.is_esp32:
        # https://github.com/ESP32Async/AsyncTCP
        from esphome.components.esp32 import add_idf_component

        add_idf_component(name="esp32async/asynctcp", ref="3.4.91")
    elif CORE.is_libretiny:
        # https://github.com/ESP32Async/AsyncTCP
        cg.add_library("ESP32Async/AsyncTCP", "3.5.0")
        # AsyncTCP 3.5.0 logs xPortGetCoreID(), which LibreTiny (single core) lacks, when LibreTiny's log_d is
        # compiled in; the call becomes int(), 0 (a function-like -D does not survive PlatformIO's build flags)
        if _libretiny_logs_debug():
            cg.add_build_flag("-DxPortGetCoreID=int")
    elif CORE.is_esp8266:
        # https://github.com/ESP32Async/ESPAsyncTCP
        cg.add_library("ESP32Async/ESPAsyncTCP", "2.0.0")
    elif CORE.is_rp2:
        # https://github.com/ayushsharma82/RPAsyncTCP
        # RPAsyncTCP is a drop-in replacement for AsyncTCP_RP2040W with better
        # ESPAsyncWebServer compatibility
        cg.add_library("ayushsharma82/RPAsyncTCP", "1.3.2")
    # Other platforms (host, etc) use socket-based implementation


def FILTER_SOURCE_FILES() -> list[str]:
    # Exclude socket implementation for platforms that use AsyncTCP libraries
    if CORE.is_esp32 or CORE.is_esp8266 or CORE.is_rp2 or CORE.is_libretiny:
        return ["async_tcp_socket.cpp"]
    return []
