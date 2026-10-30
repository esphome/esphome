from dataclasses import dataclass, field
import logging
from pathlib import Path

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import CONF_FRAMEWORK, CONF_ID, PlatformFramework
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.helpers import copy_file_if_changed
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]
DOMAIN = "web_server_base"


def AUTO_LOAD() -> list[str]:
    if CORE.is_esp32:
        return ["web_server_idf"]
    if CORE.using_arduino:
        return ["async_tcp"]
    return []


web_server_base_ns = cg.esphome_ns.namespace("web_server_base")
WebServerBase = web_server_base_ns.class_("WebServerBase")

CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

# ESP-IDF's HTTPD_DEFAULT_CONFIG max_open_sockets. Kept as the floor for the httpd
# client limit so a config that registers fewer clients behaves as before.
DEFAULT_HTTPD_MAX_OPEN_SOCKETS = 7
# The ESP-IDF httpd server opens 2 UDP sockets of its own to pass control messages.
HTTPD_CONTROL_UDP_SOCKETS = 2
# httpd_start() refuses max_open_sockets above CONFIG_LWIP_MAX_SOCKETS minus this:
# the listening socket and the 2 UDP control sockets.
HTTPD_INTERNAL_SOCKETS = 1 + HTTPD_CONTROL_UDP_SOCKETS


@dataclass
class WebServerBaseData:
    # Client connections each consumer keeps open on the shared HTTP server.
    httpd_clients: dict[str, int] = field(default_factory=dict)


def _get_data() -> WebServerBaseData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = WebServerBaseData()
    return CORE.data[DOMAIN]


def add_httpd_clients(count: int, consumer: str) -> None:
    """Register client connections that consumer keeps open on the shared HTTP server.

    Counts them as TCP sockets for the network stack and adds them to the client
    limit of the ESP-IDF httpd server (max_open_sockets). Call during config
    validation, before web_server_base's to_code runs.
    """
    from esphome.components import socket

    socket.consume_sockets(count, consumer)({})
    clients = _get_data().httpd_clients
    clients[consumer] = clients.get(consumer, 0) + count


def get_httpd_client_count() -> int:
    """Total client connections registered with add_httpd_clients()."""
    return sum(_get_data().httpd_clients.values())


def consume_captive_dns_sockets(config: ConfigType, name: str) -> None:
    """Register the sockets a captive portal needs on top of the shared HTTP server:
    1 UDP socket for the DNS server and 3 HTTP clients for the OS captive portal probes,
    which make several requests that linger in TIME_WAIT."""
    from esphome.components import socket

    add_httpd_clients(3, name)
    socket.consume_sockets(1, name, socket.SocketType.UDP)(config)


def add_captive_dns_library() -> None:
    """Pull in the Arduino DNSServer library used by CaptiveDNS off ESP32."""
    if CORE.using_arduino and (CORE.is_esp8266 or CORE.is_libretiny or CORE.is_rp2):
        cg.add_library("DNSServer", None)


def _consume_web_server_base_sockets(config: ConfigType) -> ConfigType:
    """Register the shared HTTP server's own sockets.

    web_server_base is the shared HTTP server used by web_server and captive_portal.
    Its listening socket (and on ESP32 the httpd control sockets) are registered here
    rather than in each consumer; consumers register their clients with
    add_httpd_clients().
    """
    from esphome.components import socket

    socket.consume_sockets(1, DOMAIN, socket.SocketType.TCP_LISTEN)(config)
    if CORE.is_esp32:
        socket.consume_sockets(
            HTTPD_CONTROL_UDP_SOCKETS, DOMAIN, socket.SocketType.UDP
        )(config)
    return config


def _httpd_max_open_sockets() -> int:
    """Client limit for the ESP-IDF httpd server: the registered clients, at least the
    ESP-IDF default, and no more than a user-set CONFIG_LWIP_MAX_SOCKETS allows."""
    from esphome.components import esp32

    clients = get_httpd_client_count()
    max_open = max(DEFAULT_HTTPD_MAX_OPEN_SOCKETS, clients)
    # Without a user override esp32 sizes CONFIG_LWIP_MAX_SOCKETS from the socket
    # registry, which already holds these clients plus the 3 internal sockets.
    user_max = CORE.config[esp32.DOMAIN][CONF_FRAMEWORK][
        esp32.CONF_SDKCONFIG_OPTIONS
    ].get("CONFIG_LWIP_MAX_SOCKETS")
    if user_max is None:
        return max_open
    try:
        limit = int(str(user_max)) - HTTPD_INTERNAL_SOCKETS
    except ValueError:
        return max_open
    if max_open > limit:
        _LOGGER.warning(
            "CONFIG_LWIP_MAX_SOCKETS=%s leaves room for only %d HTTP clients, "
            "%d are registered; limiting the web server to %d",
            user_max,
            limit,
            clients,
            limit,
        )
        max_open = limit
    return max_open


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
        cg.add_define("WEB_SERVER_MAX_OPEN_SOCKETS", _httpd_max_open_sockets())
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
