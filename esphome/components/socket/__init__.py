from collections.abc import Callable, MutableMapping
from dataclasses import dataclass
from enum import StrEnum
import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]

CONF_IMPLEMENTATION = "implementation"
IMPLEMENTATION_LWIP_TCP = "lwip_tcp"
IMPLEMENTATION_LWIP_SOCKETS = "lwip_sockets"
IMPLEMENTATION_BSD_SOCKETS = "bsd_sockets"

# Socket tracking infrastructure
# Components register their socket needs and platforms read this to configure appropriately
KEY_SOCKET_CONSUMERS_TCP = "socket_consumers_tcp"
KEY_SOCKET_CONSUMERS_UDP = "socket_consumers_udp"
KEY_SOCKET_CONSUMERS_TCP_LISTEN = "socket_consumers_tcp_listen"

# Recommended minimum socket counts.
# Platforms should apply these (or their own) on top of get_socket_counts().
# These cover minimal configs (e.g. api-only without web_server).
# When web_server is present, its 5 registered sockets push past the TCP minimum.
MIN_TCP_SOCKETS = 8
MIN_UDP_SOCKETS = 6
# Minimum listening sockets — at least api + ota baseline.
MIN_TCP_LISTEN_SOCKETS = 2

# Wake loop threadsafe support tracking
KEY_WAKE_LOOP_THREADSAFE_REQUIRED = "wake_loop_threadsafe_required"


class SocketType(StrEnum):
    TCP = "tcp"
    UDP = "udp"
    TCP_LISTEN = "tcp_listen"


_SOCKET_TYPE_KEYS = {
    SocketType.TCP: KEY_SOCKET_CONSUMERS_TCP,
    SocketType.UDP: KEY_SOCKET_CONSUMERS_UDP,
    SocketType.TCP_LISTEN: KEY_SOCKET_CONSUMERS_TCP_LISTEN,
}


def consume_sockets(
    value: int, consumer: str, socket_type: SocketType = SocketType.TCP
) -> Callable[[MutableMapping], MutableMapping]:
    """Register socket usage for a component.

    Args:
        value: Number of sockets needed by the component
        consumer: Name of the component consuming the sockets
        socket_type: Type of socket (SocketType.TCP, SocketType.UDP, or SocketType.TCP_LISTEN)

    Returns:
        A validator function that records the socket usage
    """
    typed_key = _SOCKET_TYPE_KEYS[socket_type]

    def _consume_sockets(config: MutableMapping) -> MutableMapping:
        consumers: dict[str, int] = CORE.data.setdefault(typed_key, {})
        consumers[consumer] = consumers.get(consumer, 0) + value
        return config

    return _consume_sockets


def _format_consumers(consumers: dict[str, int]) -> str:
    """Format consumer dict as 'name=count, ...' or 'none'."""
    if not consumers:
        return "none"
    return ", ".join(f"{name}={count}" for name, count in sorted(consumers.items()))


@dataclass(frozen=True)
class SocketCounts:
    """Socket counts and component details for platform configuration."""

    tcp: int
    udp: int
    tcp_listen: int
    tcp_details: str
    udp_details: str
    tcp_listen_details: str


def get_socket_counts() -> SocketCounts:
    """Return socket counts and component details for platform configuration.

    Platforms call this during code generation to configure lwIP socket limits.
    All components will have registered their needs by then.

    Platforms should apply their own minimums on top of these values.
    """
    tcp_consumers = CORE.data.get(KEY_SOCKET_CONSUMERS_TCP, {})
    udp_consumers = CORE.data.get(KEY_SOCKET_CONSUMERS_UDP, {})
    tcp_listen_consumers = CORE.data.get(KEY_SOCKET_CONSUMERS_TCP_LISTEN, {})
    tcp = sum(tcp_consumers.values())
    udp = sum(udp_consumers.values())
    tcp_listen = sum(tcp_listen_consumers.values())

    tcp_details = _format_consumers(tcp_consumers)
    udp_details = _format_consumers(udp_consumers)
    tcp_listen_details = _format_consumers(tcp_listen_consumers)
    _LOGGER.debug(
        "Socket counts: TCP=%d (%s), UDP=%d (%s), TCP_LISTEN=%d (%s)",
        tcp,
        tcp_details,
        udp,
        udp_details,
        tcp_listen,
        tcp_listen_details,
    )
    return SocketCounts(
        tcp, udp, tcp_listen, tcp_details, udp_details, tcp_listen_details
    )


def require_wake_loop_threadsafe() -> None:
    """Mark that wake_loop_threadsafe support is required by a component.

    Call this from components that need to wake the main event loop from background threads.
    This enables the shared UDP loopback socket mechanism (~208 bytes RAM).
    The socket is shared across all components that use this feature.

    This call is a no-op if networking is not enabled in the configuration.

    IMPORTANT: This is for background thread context only, NOT ISR context.
    Socket operations are not safe to call from ISR handlers.

    On ESP32, FreeRTOS task notifications are used instead (no socket needed).

    Example:
        from esphome.components import socket

        async def to_code(config):
            socket.require_wake_loop_threadsafe()
    """

    # Only set up once (idempotent - multiple components can call this)
    if CORE.has_networking and not CORE.data.get(
        KEY_WAKE_LOOP_THREADSAFE_REQUIRED, False
    ):
        CORE.data[KEY_WAKE_LOOP_THREADSAFE_REQUIRED] = True
        cg.add_define("USE_WAKE_LOOP_THREADSAFE")
        if not CORE.is_esp32 and not CORE.is_libretiny:
            # Only platforms without fast select need a UDP socket for wake
            # notifications. ESP32 and LibreTiny use FreeRTOS task notifications
            # instead (no socket needed).
            consume_sockets(1, "socket.wake_loop_threadsafe", SocketType.UDP)({})


CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(
            CONF_IMPLEMENTATION,
            esp8266=IMPLEMENTATION_LWIP_TCP,
            esp32=IMPLEMENTATION_BSD_SOCKETS,
            rp2040=IMPLEMENTATION_LWIP_TCP,
            bk72xx=IMPLEMENTATION_LWIP_SOCKETS,
            ln882x=IMPLEMENTATION_LWIP_SOCKETS,
            rtl87xx=IMPLEMENTATION_LWIP_SOCKETS,
            host=IMPLEMENTATION_BSD_SOCKETS,
        ): cv.one_of(
            IMPLEMENTATION_LWIP_TCP,
            IMPLEMENTATION_LWIP_SOCKETS,
            IMPLEMENTATION_BSD_SOCKETS,
            lower=True,
            space="_",
        ),
    }
)


async def to_code(config):
    impl = config[CONF_IMPLEMENTATION]
    if impl == IMPLEMENTATION_LWIP_TCP:
        cg.add_define("USE_SOCKET_IMPL_LWIP_TCP")
    elif impl == IMPLEMENTATION_LWIP_SOCKETS:
        cg.add_define("USE_SOCKET_IMPL_LWIP_SOCKETS")
        cg.add_define("USE_SOCKET_SELECT_SUPPORT")
    elif impl == IMPLEMENTATION_BSD_SOCKETS:
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")
        cg.add_define("USE_SOCKET_SELECT_SUPPORT")
    # ESP32 and LibreTiny both have LwIP >= 2.1.3 with lwip_socket_dbg_get_socket()
    # and FreeRTOS task notifications — enable fast select to bypass lwip_select().
    # Only when not using lwip_tcp, which does not provide select() support.
    if (CORE.is_esp32 or CORE.is_libretiny) and impl != IMPLEMENTATION_LWIP_TCP:
        cg.add_build_flag("-DUSE_LWIP_FAST_SELECT")


def FILTER_SOURCE_FILES() -> list[str]:
    """Return list of socket implementation files that aren't selected by the user."""
    impl = CORE.config["socket"][CONF_IMPLEMENTATION]

    # Build list of files to exclude based on selected implementation
    excluded = []
    if impl != IMPLEMENTATION_LWIP_TCP:
        excluded.append("lwip_raw_tcp_impl.cpp")
    if impl != IMPLEMENTATION_BSD_SOCKETS:
        excluded.append("bsd_sockets_impl.cpp")
    if impl != IMPLEMENTATION_LWIP_SOCKETS:
        excluded.append("lwip_sockets_impl.cpp")
    return excluded
