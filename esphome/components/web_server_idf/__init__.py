from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components.esp32 import (
    include_builtin_idf_component,
    request_tls,
    require_mbedtls_tls_server,
    set_idf_sdkconfig_default,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@dentra"]
DOMAIN = "web_server_idf"

# Components that use the shared server in ways TLS does not support yet:
# web_server streams events from the main loop straight to the socket,
# phones check for a captive portal over plain HTTP, and prometheus is
# advertised over mDNS as a plain HTTP service.
SSL_INCOMPATIBLE_COMPONENTS = ("web_server", "captive_portal", "prometheus")

# ESP-IDF's HTTPS server allows this many open connections; each holds a TLS session.
HTTPS_MAX_CONNECTIONS = 4


@dataclass
class WebServerIdfData:
    ssl: bool = False  # a component serves HTTPS (consume_ssl_sockets() was called)


def _get_data() -> WebServerIdfData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = WebServerIdfData()
    return CORE.data[DOMAIN]


def consume_ssl_sockets(config: ConfigType) -> ConfigType:
    """Register the client sockets the HTTPS server can hold open.

    Components that call enable_ssl() themselves add this to their schema.
    It also checks the configuration for components that can't share the
    server with HTTPS.
    """
    from esphome.components import socket

    data = _get_data()
    if not data.ssl:
        # The listener is shared, so its connections are reserved once however many components use it.
        socket.consume_sockets(HTTPS_MAX_CONNECTIONS, DOMAIN)(config)
        data.ssl = True
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_on_esp32,
)


def _final_validate(config: ConfigType) -> None:
    if not _get_data().ssl:
        return
    full_config = fv.full_config.get()
    for component in SSL_INCOMPATIBLE_COMPONENTS:
        if component in full_config:
            raise cv.Invalid(f"HTTPS can't be used together with '{component}' yet")


FINAL_VALIDATE_SCHEMA = _final_validate


def enable_ssl() -> None:
    """Compile in HTTPS support for the shared web server.

    Components that supply their own certificate at run time call this from
    their to_code(), then call WebServerBase::set_ssl_credentials().
    """
    cg.add_define("USE_WEBSERVER_SSL")
    request_tls()
    # ESPHome builds mbedTLS client-only unless a component accepts TLS sessions.
    require_mbedtls_tls_server()
    include_builtin_idf_component("esp_https_server")
    set_idf_sdkconfig_default("CONFIG_ESP_HTTPS_SERVER_ENABLE", True)


async def to_code(config: ConfigType) -> None:
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    set_idf_sdkconfig_default("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)
    # Re-enable ESP-IDF's HTTP server (excluded by default to save compile time).
    # Basic auth uses mbedtls_base64_encode directly, so no TLS stack is needed.
    include_builtin_idf_component("esp_http_server")


# multipart.cpp is fully #ifdef'd on USE_WEBSERVER_OTA (set by the
# web_server OTA platform); skip it when OTA uploads are not configured.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"multipart.cpp": "USE_WEBSERVER_OTA"}
)
