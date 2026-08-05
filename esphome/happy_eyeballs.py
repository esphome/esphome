"""Happy Eyeballs (RFC 8305) connection support for requests/urllib3.

urllib3 connects to each address from getaddrinfo in sequence, giving every
attempt the full connect timeout. On networks that advertise IPv6 DNS
without working IPv6 connectivity, every download stalls on the unreachable
IPv6 address until the timeout expires before IPv4 is even tried, turning
quick validation checks into multi-minute waits.

``ensure_happy_eyeballs()`` swaps urllib3's ``create_connection`` for one
that races address families with a short stagger using aiohappyeyeballs
(already in the dependency tree via aioesphomeapi), so a working address
wins in milliseconds regardless of broken families. The async connect runs
on a daemon-thread event loop via ``run_async`` so callers stay synchronous.
"""

from __future__ import annotations

import asyncio
import logging
import socket
from typing import TYPE_CHECKING, Any

from esphome.async_thread import run_async

if TYPE_CHECKING:
    from collections.abc import Callable

_LOGGER = logging.getLogger(__name__)

# RFC 8305 recommended delay between staggered connection attempts.
HAPPY_EYEBALLS_DELAY = 0.25

# Extra seconds the connect thread gets beyond the connect timeout before
# the caller gives up waiting for it.
_THREAD_WAIT_BUFFER = 5.0


def ensure_happy_eyeballs() -> None:
    """Make urllib3 (and therefore requests) connect with Happy Eyeballs.

    Idempotent; call before performing requests-based downloads.
    """
    try:
        import urllib3.util.connection

        if getattr(
            urllib3.util.connection.create_connection, "_esphome_patched", False
        ):
            return

        urllib3.util.connection.create_connection = _make_create_connection()
    except (ImportError, AttributeError) as err:  # urllib3 internals moved
        # Warning, not debug: the degraded mode brings back the very stalls
        # this module exists to prevent, so it must be visible in a normal
        # run, not only with -l DEBUG.
        _LOGGER.warning(
            "Happy Eyeballs unavailable (%s); downloads use the slower stock "
            "urllib3 connect",
            err,
        )
        _LOGGER.debug("Happy Eyeballs fallback traceback", exc_info=True)


def _make_create_connection() -> Callable[..., socket.socket]:
    """Build a drop-in replacement for urllib3's ``create_connection``."""
    from aiohappyeyeballs import start_connection
    from urllib3.exceptions import LocationParseError
    from urllib3.util.connection import (  # noqa: PLC2701
        _set_socket_options,
        allowed_gai_family,
    )
    from urllib3.util.timeout import _DEFAULT_TIMEOUT  # noqa: PLC2701

    def create_connection(
        address: tuple[str, int],
        timeout: Any = _DEFAULT_TIMEOUT,
        source_address: tuple[str, int] | None = None,
        socket_options: Any = None,
    ) -> socket.socket:
        host, port = address
        if host.startswith("["):
            host = host.strip("[]")
        try:
            host.encode("idna")
        except UnicodeError:
            raise LocationParseError(f"'{host}', label empty or too long") from None

        addr_infos = socket.getaddrinfo(
            host, port, allowed_gai_family(), socket.SOCK_STREAM
        )
        if not addr_infos:
            # Match stock urllib3 so the failure stays inside the
            # requests exception hierarchy.
            raise OSError("getaddrinfo returns an empty list")
        connect_timeout = (
            socket.getdefaulttimeout() if timeout is _DEFAULT_TIMEOUT else timeout
        )

        def socket_factory(addr_info: Any) -> socket.socket:
            family, type_, proto, _, _ = addr_info
            sock = socket.socket(family, type_, proto)
            try:
                _set_socket_options(sock, socket_options)
                if source_address:
                    sock.bind(source_address)
            except BaseException:
                sock.close()
                raise
            return sock

        async def connect() -> socket.socket:
            return await asyncio.wait_for(
                start_connection(
                    addr_infos,
                    happy_eyeballs_delay=HAPPY_EYEBALLS_DELAY,
                    interleave=1,
                    socket_factory=socket_factory,
                ),
                connect_timeout,
            )

        wait = (
            None if connect_timeout is None else connect_timeout + _THREAD_WAIT_BUFFER
        )
        # on_orphan closes any socket the abandoned thread wins after the
        # outer timeout, so a starved event loop cannot leak a live
        # connection.
        sock = run_async(connect, timeout=wait, on_orphan=socket.socket.close)
        # aiohappyeyeballs leaves the winning socket non-blocking; restore the
        # blocking-with-timeout behavior urllib3 callers expect.
        try:
            sock.settimeout(connect_timeout)
        except BaseException:
            sock.close()
            raise
        return sock

    create_connection._esphome_patched = True  # type: ignore[attr-defined]  # pylint: disable=protected-access
    return create_connection
