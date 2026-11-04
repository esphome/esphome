from dataclasses import dataclass, field
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
from esphome.core import CORE
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@dentra"]
DOMAIN = "web_server_idf"

# ESP-IDF's HTTPD_DEFAULT_CONFIG max_open_sockets. Kept as the floor for the httpd
# client limit so a config that registers fewer clients behaves as before.
DEFAULT_HTTPD_MAX_OPEN_SOCKETS = 7
# The ESP-IDF httpd server opens 2 UDP sockets of its own to pass control messages.
HTTPD_CONTROL_UDP_SOCKETS = 2


# ESP-IDF's HTTPS server allows this many open connections; each holds a TLS session.
HTTPS_MAX_CONNECTIONS = 4


@dataclass
class WebServerIdfData:
    ssl: bool = False  # a component serves HTTPS (consume_ssl_sockets() was called)
    # Consumers that need plain HTTP (require_plain_http() was called)
    plain_http: list[str] = field(default_factory=list)


def _get_data() -> WebServerIdfData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = WebServerIdfData()
    return CORE.data[DOMAIN]


def consume_ssl_sockets(config: ConfigType) -> ConfigType:
    """Serve the shared web server over HTTPS.

    The one thing a component that supplies its own certificate adds to its
    schema; it then calls WebServerBase::set_ssl_credentials() at run time.
    This compiles HTTPS in and registers the connections it can hold open.
    """
    data = _get_data()
    # HTTPS exists only on ESP32. The listener is shared, so its clients are registered once.
    if CORE.is_esp32 and not data.ssl:
        web_server_base.add_httpd_clients(HTTPS_MAX_CONNECTIONS, DOMAIN)
        data.ssl = True
    return config


def require_plain_http(consumer: str) -> None:
    """Record that consumer needs the shared web server to speak plain HTTP.

    Call during config validation; HTTPS is then rejected in final validation.
    """
    _get_data().plain_http.append(consumer)


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
    # Each TLS session takes memory, so HTTPS keeps ESP-IDF's HTTPS connection limit.
    if _get_data().ssl:
        max_open = min(max_open, HTTPS_MAX_CONNECTIONS)
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
    # Runs after every CONFIG_SCHEMA, so all require_plain_http() calls are in.
    data = _get_data()
    if data.ssl and data.plain_http:
        names = ", ".join(data.plain_http)
        raise cv.Invalid(f"HTTPS can't be used together with {names} yet")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    # Runs after config validation, so every add_httpd_clients() call is counted.
    cg.add_define("WEB_SERVER_MAX_OPEN_SOCKETS", _httpd_max_open_sockets())
    if _get_data().ssl:
        cg.add_define("USE_WEBSERVER_SSL")
        request_tls()
        # ESPHome builds mbedTLS client-only unless a component accepts TLS sessions.
        require_mbedtls_tls_server()
        include_builtin_idf_component("esp_https_server")
        set_idf_sdkconfig_default("CONFIG_ESP_HTTPS_SERVER_ENABLE", True)
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
