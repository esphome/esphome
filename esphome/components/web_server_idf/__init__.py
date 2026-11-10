import logging

import esphome.codegen as cg
from esphome.components import socket, web_server_base
from esphome.components.esp32 import (
    get_user_lwip_max_sockets,
    include_builtin_idf_component,
    set_idf_sdkconfig_default,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@dentra"]
DOMAIN = "web_server_idf"

# ESP-IDF's HTTPD_DEFAULT_CONFIG max_open_sockets. Kept as the floor for the httpd
# client limit so a config that registers fewer clients behaves as before.
DEFAULT_HTTPD_MAX_OPEN_SOCKETS = 7
# The ESP-IDF httpd server opens 2 UDP sockets of its own to pass control messages.
HTTPD_CONTROL_UDP_SOCKETS = 2


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
