from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.helpers import copy_file_if_changed

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]


def AUTO_LOAD():
    if CORE.is_esp32:
        return ["web_server_idf"]
    if CORE.using_arduino:
        return ["async_tcp"]
    return []


web_server_base_ns = cg.esphome_ns.namespace("web_server_base")
WebServerBase = web_server_base_ns.class_("WebServerBase", cg.Component)

CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WebServerBase),
    }
)


@coroutine_with_priority(CoroPriority.WEB_SERVER_BASE)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
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
        if CORE.is_rp2040:
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
