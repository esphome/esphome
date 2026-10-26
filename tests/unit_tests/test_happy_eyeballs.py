"""Tests for the Happy Eyeballs urllib3 shim."""

from __future__ import annotations

import asyncio
from collections.abc import Generator
from concurrent.futures import ThreadPoolExecutor
import socket
import threading
from typing import Any
from unittest.mock import Mock, patch

import pytest

from esphome.happy_eyeballs import _make_create_connection, ensure_happy_eyeballs


def _addr_info(host: str, port: int) -> tuple[Any, ...]:
    """Build a getaddrinfo-style result tuple for an IPv4 address."""
    return (socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP, "", (host, port))


@pytest.fixture
def create_connection() -> Any:
    """A freshly built Happy Eyeballs create_connection replacement."""
    return _make_create_connection()


@pytest.fixture
def listener() -> Generator[tuple[str, int]]:
    """A listening TCP socket on localhost; yields its address."""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(("127.0.0.1", 0))
    server.listen(5)
    yield server.getsockname()
    server.close()


@pytest.fixture
def mock_gai(listener: tuple[str, int]) -> Generator[Any]:
    """Resolve every host to two copies of the listener's address."""
    with patch("socket.getaddrinfo", return_value=[_addr_info(*listener)] * 2) as mock:
        yield mock


def test_ensure_happy_eyeballs_patches_and_is_idempotent(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The shim replaces urllib3's create_connection exactly once."""
    import urllib3.util.connection

    def stock(*args: Any, **kwargs: Any) -> None:
        pass

    monkeypatch.setattr(urllib3.util.connection, "create_connection", stock)

    ensure_happy_eyeballs()
    patched = urllib3.util.connection.create_connection
    assert patched is not stock
    assert patched._esphome_patched

    ensure_happy_eyeballs()
    assert urllib3.util.connection.create_connection is patched


def test_ensure_happy_eyeballs_concurrent_first_calls_patch_once(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Worker threads fanning out (download_content_many, run_batch_downloads)
    may race the first call; the replacement is built exactly once."""
    import urllib3.util.connection

    from esphome import happy_eyeballs

    def stock(*args: Any, **kwargs: Any) -> None:
        pass

    monkeypatch.setattr(urllib3.util.connection, "create_connection", stock)

    barrier = threading.Barrier(8)
    builds: list[int] = []
    real_make = happy_eyeballs._make_create_connection

    def counting_make() -> Any:
        builds.append(1)
        return real_make()

    monkeypatch.setattr(happy_eyeballs, "_make_create_connection", counting_make)

    def racer() -> None:
        barrier.wait(timeout=10)
        ensure_happy_eyeballs()

    with ThreadPoolExecutor(max_workers=8) as ex:
        list(ex.map(lambda _: racer(), range(8)))

    assert builds == [1]
    assert urllib3.util.connection.create_connection._esphome_patched


def test_connects_and_restores_socket_state(
    create_connection: Any, listener: tuple[str, int], mock_gai: Any
) -> None:
    """The winning socket comes back blocking, with timeout and options set."""
    sock = create_connection(
        ("example.com", listener[1]),
        timeout=5,
        socket_options=[(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)],
    )

    try:
        assert sock.getpeername() == listener
        assert sock.gettimeout() == 5
        assert sock.getsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY) != 0
    finally:
        sock.close()


def test_single_address_connects(
    create_connection: Any, listener: tuple[str, int]
) -> None:
    """A host resolving to one address connects through the same path."""
    with patch("socket.getaddrinfo", return_value=[_addr_info(*listener)]):
        sock = create_connection(("example.com", listener[1]), timeout=5)

    try:
        assert sock.getpeername() == listener
    finally:
        sock.close()


def test_falls_back_to_working_address(
    create_connection: Any, listener: tuple[str, int], monkeypatch: pytest.MonkeyPatch
) -> None:
    """An unreachable first address does not block the working one."""
    from esphome import happy_eyeballs

    # 192.0.2.1 (TEST-NET-1) blackholes or fails fast depending on the
    # network; either way the second address must win well within the
    # timeout instead of waiting out the first. A short stagger keeps the
    # test's duration network independent.
    monkeypatch.setattr(happy_eyeballs, "HAPPY_EYEBALLS_DELAY", 0.01)
    addr_infos = [_addr_info("192.0.2.1", 9), _addr_info(*listener)]

    with patch("socket.getaddrinfo", return_value=addr_infos):
        sock = create_connection(("example.com", listener[1]), timeout=10)

    try:
        assert sock.getpeername() == listener
    finally:
        sock.close()


def test_bracketed_ipv6_host_is_stripped(
    create_connection: Any, listener: tuple[str, int], mock_gai: Any
) -> None:
    """A bracketed IPv6 literal is unbracketed before resolution."""
    sock = create_connection(("[::1]", listener[1]), timeout=5)

    try:
        assert mock_gai.call_args[0][0] == "::1"
        assert sock.getpeername() == listener
    finally:
        sock.close()


def test_source_address_is_bound(
    create_connection: Any, listener: tuple[str, int], mock_gai: Any
) -> None:
    """The socket binds to the requested source address before connecting."""
    sock = create_connection(
        ("example.com", listener[1]),
        timeout=5,
        source_address=("127.0.0.1", 0),
    )

    try:
        assert sock.getsockname()[0] == "127.0.0.1"
    finally:
        sock.close()


def test_socket_factory_failure_closes_socket(
    listener: tuple[str, int], mock_gai: Any
) -> None:
    """A socket-option failure fails the connect instead of leaking sockets.

    Instrumented at ``_set_socket_options`` (which the factory calls with
    the just-created socket) rather than by patching ``socket.socket``,
    which is platform dependent: the event loop's internal socketpair use
    differs between platforms.
    """
    created: list[socket.socket] = []

    def failing_set_options(sock: socket.socket, options: Any) -> None:
        created.append(sock)
        raise OSError("bad socket option")

    # Patch before building the closure; it binds _set_socket_options at
    # creation time.
    with patch("urllib3.util.connection._set_socket_options", new=failing_set_options):
        create_connection = _make_create_connection()
        with pytest.raises(OSError):
            create_connection(
                ("example.com", listener[1]),
                timeout=5,
                socket_options=[(999999, 999999, 1)],
            )

    assert created, "socket factory never ran"
    assert all(sock.fileno() == -1 for sock in created), "socket leaked open"


def test_default_timeout_yields_blocking_socket(
    create_connection: Any, listener: tuple[str, int], mock_gai: Any
) -> None:
    """Without an explicit timeout the socket follows the global default."""
    sock = create_connection(("example.com", listener[1]))

    try:
        assert sock.gettimeout() is socket.getdefaulttimeout()
    finally:
        sock.close()


def test_settimeout_failure_closes_socket(
    create_connection: Any, mock_gai: Any
) -> None:
    """A failure restoring socket state closes the winner instead of leaking."""
    bad_sock = Mock()
    bad_sock.settimeout.side_effect = OSError("bad timeout")

    with (
        patch("esphome.async_thread.run_async", return_value=bad_sock),
        pytest.raises(OSError, match="bad timeout"),
    ):
        create_connection(("example.com", 80), timeout=5)

    bad_sock.close.assert_called_once()


def test_connect_timeout_raises() -> None:
    """A connect that never completes raises within the timeout."""

    async def never(*args: Any, **kwargs: Any) -> None:
        await asyncio.sleep(60)

    addr_infos = [_addr_info("192.0.2.1", 9), _addr_info("192.0.2.2", 9)]

    # Patch before building the closure; it binds start_connection at
    # creation time.
    with patch("aiohappyeyeballs.start_connection", new=never):
        create_connection = _make_create_connection()
        with (
            patch("socket.getaddrinfo", return_value=addr_infos),
            pytest.raises(TimeoutError),
        ):
            create_connection(("example.com", 80), timeout=0.1)


def test_invalid_host_raises_location_parse_error(create_connection: Any) -> None:
    """Hostnames urllib3 would reject are still rejected."""
    from urllib3.exceptions import LocationParseError

    with pytest.raises(LocationParseError):
        create_connection(("a" * 300, 80))


def test_empty_getaddrinfo_raises_oserror(create_connection: Any) -> None:
    """An empty resolution matches stock urllib3's OSError, not ValueError."""
    with (
        patch("socket.getaddrinfo", return_value=[]),
        pytest.raises(OSError, match="empty"),
    ):
        create_connection(("example.com", 80), timeout=5)


def test_ensure_falls_back_to_stock_when_internals_move(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """If urllib3 private names disappear, downloads keep the stock connect
    and the warning is latched to fire once, not per download."""
    import urllib3.util.connection

    from esphome import happy_eyeballs

    def stock(*args: Any, **kwargs: Any) -> None:
        pass

    factory = Mock(side_effect=ImportError("gone"))
    monkeypatch.setattr(urllib3.util.connection, "create_connection", stock)
    monkeypatch.setattr(happy_eyeballs, "_make_create_connection", factory)

    ensure_happy_eyeballs()
    ensure_happy_eyeballs()
    assert urllib3.util.connection.create_connection is stock
    assert factory.call_count == 1
    assert caplog.text.count("Happy Eyeballs unavailable") == 1


def test_ensure_survives_missing_urllib3(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """An unimportable urllib3 degrades with a warning instead of raising."""
    import sys

    with patch.dict(sys.modules, {"urllib3.util.connection": None}):
        ensure_happy_eyeballs()
    assert "Happy Eyeballs unavailable" in caplog.text


def test_requests_routes_through_shim(monkeypatch: pytest.MonkeyPatch) -> None:
    """Patching urllib3's create_connection actually reroutes requests."""
    from http.server import BaseHTTPRequestHandler, HTTPServer
    import threading

    import requests
    import urllib3.util.connection

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            self.send_response(200)
            self.send_header("Content-Length", "2")
            self.end_headers()
            self.wfile.write(b"ok")

        def log_message(self, *args: Any) -> None:
            pass

    server = HTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    host, port = server.server_address

    calls: list[Any] = []
    shim = _make_create_connection()

    def counting(*args: Any, **kwargs: Any) -> Any:
        calls.append(args)
        return shim(*args, **kwargs)

    counting._esphome_patched = True
    monkeypatch.setattr(urllib3.util.connection, "create_connection", counting)

    real_getaddrinfo = socket.getaddrinfo

    def fake_getaddrinfo(h: str, p: int, *args: Any, **kwargs: Any) -> Any:
        if h == "shim-test.invalid":
            return [_addr_info(host, port), _addr_info(host, port)]
        return real_getaddrinfo(h, p, *args, **kwargs)

    monkeypatch.setattr(socket, "getaddrinfo", fake_getaddrinfo)

    try:
        with requests.Session() as session:
            session.trust_env = False
            resp = session.get(f"http://shim-test.invalid:{port}/", timeout=5)
        assert resp.status_code == 200
        assert resp.content == b"ok"
        assert calls, "requests did not go through the patched create_connection"
    finally:
        server.shutdown()
        server.server_close()
