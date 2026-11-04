from dataclasses import dataclass
import logging

import esphome.codegen as cg
from esphome.components import socket, web_server_base
from esphome.components.esp32 import (
    get_user_lwip_max_sockets,
    include_builtin_idf_component,
    request_tls,
    require_mbedtls_tls_server,
    set_idf_sdkconfig_default,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@dentra"]
DOMAIN = "web_server_idf"

# ESP-IDF's HTTPD_DEFAULT_CONFIG max_open_sockets. Kept as the floor for the httpd
# client limit so a config that registers fewer clients behaves as before.
DEFAULT_HTTPD_MAX_OPEN_SOCKETS = 7
# The ESP-IDF httpd server opens 2 UDP sockets of its own to pass control messages.
HTTPD_CONTROL_UDP_SOCKETS = 2


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
    data = _get_data()
    # HTTPS exists only on ESP32; elsewhere there are no connections to reserve.
    if CORE.is_esp32 and not data.ssl:
        # The listener is shared, so its connections are reserved once however many components use it.
        socket.consume_sockets(HTTPS_MAX_CONNECTIONS, DOMAIN)(config)
        data.ssl = True
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_on_esp32,
    # The listening socket is registered by web_server_base.
    socket.consume_sockets(HTTPD_CONTROL_UDP_SOCKETS, DOMAIN, socket.SocketType.UDP),
)


def _httpd_max_open_sockets() -> int:
    """Client limit for the httpd server: the registered clients, at least the ESP-IDF
    default, and no more than a user-set CONFIG_LWIP_MAX_SOCKETS allows."""
    max_open = max(
        DEFAULT_HTTPD_MAX_OPEN_SOCKETS, web_server_base.get_httpd_client_count()
    )
    # Without a user override esp32 sizes CONFIG_LWIP_MAX_SOCKETS from the socket
    # registry, which already holds the clients and the internal sockets.
    if (user_max := get_user_lwip_max_sockets()) is None:
        return max_open
    # httpd_start() refuses max_open_sockets above CONFIG_LWIP_MAX_SOCKETS minus the
    # listening socket and the 2 UDP control sockets. esp32 already warns when the
    # user's value is below what the config needs.
    limit = max(1, user_max - 1 - HTTPD_CONTROL_UDP_SOCKETS)
    if max_open > limit:
        _LOGGER.info(
            "CONFIG_LWIP_MAX_SOCKETS=%d limits the web server to %d clients",
            user_max,
            limit,
        )
        max_open = limit
    return max_open


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
    their to_code(), then call WebServerBase::set_ssl_credentials(). The
    component's schema must include consume_ssl_sockets().
    """
    if not _get_data().ssl:
        raise EsphomeError(
            "web_server_idf.enable_ssl() needs web_server_idf.consume_ssl_sockets() in the calling component's schema"
        )
    cg.add_define("USE_WEBSERVER_SSL")
    request_tls()
    # ESPHome builds mbedTLS client-only unless a component accepts TLS sessions.
    require_mbedtls_tls_server()
    include_builtin_idf_component("esp_https_server")
    set_idf_sdkconfig_default("CONFIG_ESP_HTTPS_SERVER_ENABLE", True)


async def to_code(config: ConfigType) -> None:
    # Runs after config validation, so every add_httpd_clients() call is counted.
    cg.add_define("WEB_SERVER_MAX_OPEN_SOCKETS", _httpd_max_open_sockets())
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
