import gzip

import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
    PlatformFramework,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority


def AUTO_LOAD() -> list[str]:
    auto_load = ["web_server_base", "ota.web_server"]
    if CORE.using_esp_idf:
        auto_load.append("socket")
    return auto_load


DEPENDENCIES = ["wifi"]
CODEOWNERS = ["@esphome/core"]

CONF_CUSTOM_HTML = "custom_html_file"

captive_portal_ns = cg.esphome_ns.namespace("captive_portal")
CaptivePortal = captive_portal_ns.class_("CaptivePortal", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CaptivePortal),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_CUSTOM_HTML): cv.file_,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_BK72XX,
            PLATFORM_LN882X,
            PLATFORM_RTL87XX,
        ]
    ),
)


def add_custom_html_index_as_progmem(content: str, compress: bool = True) -> None:
    """Add a resource to progmem."""
    content_encoded = content.encode("utf-8")
    if compress:
        content_encoded = gzip.compress(content_encoded)
    content_encoded_size = len(content_encoded)
    bytes_as_int = ", ".join(str(x) for x in content_encoded)
    uint8_t = (
        f"const uint8_t ESPHOME_CAPTIVE_PORTAL_INDEX_GZ[] PROGMEM = {{{bytes_as_int}}}"
    )
    size_t = (
        f"const size_t ESPHOME_CAPTIVE_PORTAL_INDEX_GZ_SIZE = {content_encoded_size}"
    )
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(size_t))


@coroutine_with_priority(CoroPriority.CAPTIVE_PORTAL)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
    cg.add_define("USE_CAPTIVE_PORTAL")

    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("ESP32 Async UDP", None)
            cg.add_library("DNSServer", None)
            cg.add_library("WiFi", None)
        if CORE.is_esp8266:
            cg.add_library("DNSServer", None)
        if CORE.is_libretiny:
            cg.add_library("DNSServer", None)

    # captive_index.h is filtered out, so this will replace the default index
    # with the user-provided one
    if CONF_CUSTOM_HTML in config:
        cg.add_define("USE_CAPTIVE_PORTAL_CUSTOM_HTML")
        path = CORE.relative_config_path(config[CONF_CUSTOM_HTML])
        with open(file=path, encoding="utf-8") as custom_html_file:
            add_custom_html_index_as_progmem(custom_html_file.read())


def FILTER_SOURCE_FILES() -> list[str]:
    # Only compile the ESP-IDF DNS server when using ESP-IDF framework
    files_to_filter = filter_source_files_from_platform(
        {
            "dns_server_esp32_idf.cpp": {PlatformFramework.ESP32_IDF},
        }
    )()

    # captive_index.h is only needed when there is no custom html index file provided
    config = CORE.config.get("captive_portal", {})
    if CONF_CUSTOM_HTML in config:
        files_to_filter.append("captive_index.h")

    return files_to_filter
