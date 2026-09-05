from pathlib import Path

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.helpers import copy_file_if_changed
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]


def AUTO_LOAD() -> list[str]:
    if CORE.is_esp32:
        return ["web_server_idf"]
    if CORE.using_arduino:
        return ["async_tcp"]
    return []


web_server_base_ns = cg.esphome_ns.namespace("web_server_base")
WebServerBase = web_server_base_ns.class_("WebServerBase")

CONF_WEB_SERVER_BASE_ID = "web_server_base_id"


def consume_captive_dns_sockets(config: ConfigType, name: str) -> None:
    """Register the sockets a captive portal needs on top of the shared HTTP server:
    1 UDP socket for the DNS server and 3 TCP sockets for the OS captive portal probes,
    which make several requests that linger in TIME_WAIT."""
    from esphome.components import socket

    socket.consume_sockets(3, name)(config)
    socket.consume_sockets(1, name, socket.SocketType.UDP)(config)


def add_captive_dns_library() -> None:
    """Pull in the Arduino DNSServer library used by CaptiveDNS off ESP32."""
    if CORE.using_arduino and (CORE.is_esp8266 or CORE.is_libretiny or CORE.is_rp2):
        cg.add_library("DNSServer", None)


def _consume_web_server_base_sockets(config: ConfigType) -> ConfigType:
    """Register the shared listening socket for the HTTP server.

    web_server_base is the shared HTTP server used by web_server and captive_portal.
    The listening socket is registered here rather than in each consumer.
    """
    from esphome.components import socket

    socket.consume_sockets(1, "web_server_base", socket.SocketType.TCP_LISTEN)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServerBase),
        }
    ),
    _consume_web_server_base_sockets,
)


@coroutine_with_priority(CoroPriority.WEB_SERVER_BASE)
async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(cg.RawExpression(f"{web_server_base_ns}::global_web_server_base = {var}"))

    if CORE.is_esp32:
        # Count for StaticVector in web_server_idf - matches headers added in init()
        cg.add_define("WEB_SERVER_DEFAULT_HEADERS_COUNT", 1)
        return

    # ESP32 uses IDF web server (early return above), so this is for other Arduino platforms
    if CORE.using_arduino:
        if CORE.is_esp8266:
            cg.add_library("ESP8266WiFi", None)
        if CORE.is_libretiny:
            CORE.add_platformio_option("lib_ignore", ["ESPAsyncTCP", "RPAsyncTCP"])
        if CORE.is_rp2:
            # Ignore bundled AsyncTCP libraries - we use RPAsyncTCP from async_tcp component
            CORE.add_platformio_option(
                "lib_ignore", ["ESPAsyncTCP", "AsyncTCP", "AsyncTCP_RP2040W"]
            )
            # ESPAsyncWebServer uses Hash library for sha1() on RP2040
            cg.add_library("Hash", None)
            # Fix Hash.h include conflict: Crypto-no-arduino (used by dsmr)
            # provides a Hash.h that shadows the framework's Hash library.
            # Prepend the framework Hash path so it's found first.
            copy_file_if_changed(
                Path(__file__).parent / "fix_rp2040_hash.py.script",
                CORE.relative_build_path("fix_rp2040_hash.py"),
            )
            cg.add_platformio_option("extra_scripts", ["pre:fix_rp2040_hash.py"])
        # https://github.com/ESP32Async/ESPAsyncWebServer/blob/main/library.json
        cg.add_library("ESP32Async/ESPAsyncWebServer", "3.9.6")


# The DNS server used for captive portals on ESP32; other platforms use the Arduino
# DNSServer library. Its source is also guarded by USE_CAPTIVE_PORTAL / USE_WEBSERVER_CAPTIVE.
FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "dns_server_esp32_idf.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
    }
)
