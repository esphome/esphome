from collections.abc import Callable, MutableMapping

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]

CONF_IMPLEMENTATION = "implementation"
IMPLEMENTATION_LWIP_TCP = "lwip_tcp"
IMPLEMENTATION_LWIP_SOCKETS = "lwip_sockets"
IMPLEMENTATION_BSD_SOCKETS = "bsd_sockets"

# Socket tracking infrastructure
# Components register their socket needs and platforms read this to configure appropriately
KEY_SOCKET_CONSUMERS = "socket_consumers"

# Wake loop threadsafe support tracking
KEY_WAKE_LOOP_THREADSAFE_REQUIRED = "wake_loop_threadsafe_required"


def consume_sockets(
    value: int, consumer: str
) -> Callable[[MutableMapping], MutableMapping]:
    """Register socket usage for a component.

    Args:
        value: Number of sockets needed by the component
        consumer: Name of the component consuming the sockets

    Returns:
        A validator function that records the socket usage
    """

    def _consume_sockets(config: MutableMapping) -> MutableMapping:
        consumers: dict[str, int] = CORE.data.setdefault(KEY_SOCKET_CONSUMERS, {})
        consumers[consumer] = consumers.get(consumer, 0) + value
        return config

    return _consume_sockets


def require_wake_loop_threadsafe() -> None:
    """Mark that wake_loop_threadsafe support is required by a component.

    Call this from components that need to wake the main event loop from background threads.
    This enables the shared UDP loopback socket mechanism (~208 bytes RAM).
    The socket is shared across all components that use this feature.

    IMPORTANT: This is for background thread context only, NOT ISR context.
    Socket operations are not safe to call from ISR handlers.

    Example:
        from esphome.components import socket

        async def to_code(config):
            socket.require_wake_loop_threadsafe()
    """
    # Only set up once (idempotent - multiple components can call this)
    if not CORE.data.get(KEY_WAKE_LOOP_THREADSAFE_REQUIRED, False):
        CORE.data[KEY_WAKE_LOOP_THREADSAFE_REQUIRED] = True
        cg.add_define("USE_WAKE_LOOP_THREADSAFE")
        # Consume 1 socket for the shared wake notification socket
        consume_sockets(1, "socket.wake_loop_threadsafe")({})


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
