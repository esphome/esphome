"""Unit tests for esphome.web_server_logs module."""

from __future__ import annotations

from collections.abc import Iterator
import logging
import socket
from typing import Self
from unittest.mock import MagicMock

import pytest
import requests
from requests.auth import HTTPBasicAuth

from esphome import web_server_logs
from esphome.core import EsphomeError
from esphome.web_server_logs import (
    EVENTS_PATH,
    WebServerLogsError,
    _build_urls,
    _consume,
    _stream,
    run_logs,
)

# A realistic slice of the web_server /events SSE stream: an initial ping
# carrying the config, a state frame, two log frames (one multi-line), plus
# comment/id/retry lines that must be ignored.
SSE_LINES = [
    "retry: 30000",
    "id: 12345",
    "event: ping",
    'data: {"title":"dev","log":true}',
    "",
    "event: state",
    'data: {"id":"sensor-x","state":"ON"}',
    "",
    "event: log",
    "data: \x1b[0;32m[I][main:001]: hello\x1b[0m",
    "",
    ": keepalive-comment",
    "event: log",
    "data: line one",
    "data: line two",
    "",
]


class _FakeResponse:
    """Minimal stand-in for a streamed ``requests`` response."""

    def __init__(self, status_code: int, lines: list[str]) -> None:
        self.status_code = status_code
        self._lines = lines

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *exc: object) -> bool:
        return False

    def iter_lines(self) -> Iterator[bytes]:
        for line in self._lines:
            yield line.encode("utf8")


@pytest.fixture
def fake_parser() -> MagicMock:
    """A LogParser whose parse_line returns the raw line unchanged."""
    parser = MagicMock()
    parser.parse_line.side_effect = lambda line, time_str: line
    return parser


def _patch_resolve(
    monkeypatch: pytest.MonkeyPatch,
    addr_infos: list[tuple[int, int, int, str, tuple]],
) -> None:
    monkeypatch.setattr(
        "esphome.web_server_helpers.resolve_ip_address",
        lambda *args, **kwargs: addr_infos,
    )


# ---------------------------------------------------------------------------
# _build_urls
# ---------------------------------------------------------------------------


def test_build_urls_ipv4(monkeypatch: pytest.MonkeyPatch) -> None:
    """An IPv4 host resolves to a plain http://ip:port/events URL."""
    _patch_resolve(
        monkeypatch,
        [(socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.5", 80))],
    )

    assert _build_urls(["dev.local"], 80) == [
        ("192.168.1.5", f"http://192.168.1.5:80{EVENTS_PATH}")
    ]


def test_build_urls_ipv6_brackets_and_zone(monkeypatch: pytest.MonkeyPatch) -> None:
    """IPv6 literals are bracketed; link-local addresses get a %25 zone index."""
    _patch_resolve(
        monkeypatch,
        [(socket.AF_INET6, socket.SOCK_STREAM, 0, "", ("fe80::1", 8080, 0, 7))],
    )

    assert _build_urls(["dev.local"], 8080) == [
        ("fe80::1", f"http://[fe80::1%257]:8080{EVENTS_PATH}")
    ]


def test_build_urls_dedups_and_skips_unresolvable(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Duplicate resolved IPs collapse to one URL; resolve errors are skipped."""
    calls: list[str] = []

    def fake_resolve(host: str, port: int, **kwargs: object) -> list[tuple]:
        calls.append(host)
        if host == "bad":
            raise EsphomeError("nope")
        return [(socket.AF_INET, socket.SOCK_STREAM, 0, "", ("10.0.0.1", port))]

    monkeypatch.setattr("esphome.web_server_helpers.resolve_ip_address", fake_resolve)

    # "good" and "dup" both resolve to 10.0.0.1, "bad" raises.
    assert _build_urls(["good", "bad", "dup"], 80) == [
        ("10.0.0.1", f"http://10.0.0.1:80{EVENTS_PATH}")
    ]
    assert calls == ["good", "bad", "dup"]


# ---------------------------------------------------------------------------
# _consume (SSE parsing)
# ---------------------------------------------------------------------------


def test_consume_emits_only_log_frames(
    monkeypatch: pytest.MonkeyPatch, fake_parser: MagicMock
) -> None:
    """Only event: log data lines are printed; ping/state/comments are ignored."""
    printed: list[str] = []
    monkeypatch.setattr(web_server_logs, "safe_print", printed.append)

    _consume(_FakeResponse(200, SSE_LINES), fake_parser)

    assert printed == [
        "\x1b[0;32m[I][main:001]: hello\x1b[0m",
        "line one",
        "line two",
    ]


def test_consume_ignores_unterminated_trailing_frame(
    monkeypatch: pytest.MonkeyPatch, fake_parser: MagicMock
) -> None:
    """A log frame without its terminating blank line is not emitted."""
    printed: list[str] = []
    monkeypatch.setattr(web_server_logs, "safe_print", printed.append)

    _consume(_FakeResponse(200, ["event: log", "data: dangling"]), fake_parser)

    assert printed == []


# ---------------------------------------------------------------------------
# _stream
# ---------------------------------------------------------------------------


def test_stream_returns_false_when_connect_fails(
    monkeypatch: pytest.MonkeyPatch,
    fake_parser: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A failed connection logs a warning and reports not-connected."""

    def boom(*args: object, **kwargs: object) -> _FakeResponse:
        raise requests.ConnectionError("refused")

    monkeypatch.setattr(requests, "get", boom)

    with caplog.at_level(logging.WARNING):
        assert (
            _stream("http://10.0.0.1:80/events", "10.0.0.1", None, fake_parser) is False
        )
    assert "Could not connect to 10.0.0.1" in caplog.text


def test_stream_returns_true_when_established_then_dropped(
    monkeypatch: pytest.MonkeyPatch,
    fake_parser: MagicMock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A mid-stream drop after connecting reports connected so we reconnect."""
    printed: list[str] = []
    monkeypatch.setattr(web_server_logs, "safe_print", printed.append)

    class _DroppingResponse(_FakeResponse):
        def iter_lines(self) -> Iterator[bytes]:
            yield b"event: log"
            yield b"data: before-drop"
            yield b""
            raise requests.exceptions.ChunkedEncodingError("connection lost")

    monkeypatch.setattr(requests, "get", lambda *a, **kw: _DroppingResponse(200, []))

    with caplog.at_level(logging.INFO):
        assert (
            _stream("http://10.0.0.1:80/events", "10.0.0.1", None, fake_parser) is True
        )
    assert printed == ["before-drop"]
    assert "reconnecting" in caplog.text


# ---------------------------------------------------------------------------
# run_logs
# ---------------------------------------------------------------------------


def test_run_logs_streams_then_reconnects_until_interrupt(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A dropped stream reconnects; KeyboardInterrupt during the pause exits 0."""
    monkeypatch.setattr(
        web_server_logs,
        "_build_urls",
        lambda hosts, port: [("10.0.0.1", "http://10.0.0.1:80/events")],
    )
    printed: list[str] = []
    monkeypatch.setattr(web_server_logs, "safe_print", printed.append)
    monkeypatch.setattr(requests, "get", lambda *a, **kw: _FakeResponse(200, SSE_LINES))

    def stop(_delay: float) -> None:
        raise KeyboardInterrupt

    monkeypatch.setattr(web_server_logs.time, "sleep", stop)

    assert run_logs(["dev.local"], 80, None, None) == 0
    # The single stream was consumed before the reconnect pause interrupted us.
    # run_logs renders through the real LogParser, which prefixes a timestamp,
    # so assert on the payloads rather than exact equality.
    assert len(printed) == 3
    assert "[I][main:001]: hello" in printed[0]
    assert "line one" in printed[1]
    assert "line two" in printed[2]


def test_run_logs_passes_basic_auth(monkeypatch: pytest.MonkeyPatch) -> None:
    """Username + password are forwarded as HTTP Basic auth on the request."""
    monkeypatch.setattr(
        web_server_logs,
        "_build_urls",
        lambda hosts, port: [("10.0.0.1", "http://10.0.0.1:80/events")],
    )
    monkeypatch.setattr(web_server_logs, "safe_print", lambda line: None)
    captured: dict[str, object] = {}

    def fake_get(url: str, **kwargs: object) -> _FakeResponse:
        captured.update(kwargs)
        captured["url"] = url
        return _FakeResponse(200, SSE_LINES)

    monkeypatch.setattr(requests, "get", fake_get)
    monkeypatch.setattr(
        web_server_logs.time,
        "sleep",
        lambda _d: (_ for _ in ()).throw(KeyboardInterrupt()),
    )

    assert run_logs(["dev.local"], 80, "admin", "secret") == 0
    auth = captured["auth"]
    assert isinstance(auth, HTTPBasicAuth)
    assert (auth.username, auth.password) == ("admin", "secret")
    assert captured["stream"] is True
    assert captured["headers"] == {"Accept": "text/event-stream"}


def test_run_logs_no_auth_when_credentials_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """No auth object is sent when username/password are not configured."""
    monkeypatch.setattr(
        web_server_logs,
        "_build_urls",
        lambda hosts, port: [("10.0.0.1", "http://10.0.0.1:80/events")],
    )
    monkeypatch.setattr(web_server_logs, "safe_print", lambda line: None)
    captured: dict[str, object] = {}

    def fake_get(url: str, **kwargs: object) -> _FakeResponse:
        captured.update(kwargs)
        return _FakeResponse(200, SSE_LINES)

    monkeypatch.setattr(requests, "get", fake_get)
    monkeypatch.setattr(
        web_server_logs.time,
        "sleep",
        lambda _d: (_ for _ in ()).throw(KeyboardInterrupt()),
    )

    assert run_logs(["dev.local"], 80, None, None) == 0
    assert captured["auth"] is None


def test_run_logs_raises_on_auth_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    """HTTP 401 aborts with a clear error rather than reconnecting forever."""
    monkeypatch.setattr(
        web_server_logs,
        "_build_urls",
        lambda hosts, port: [("10.0.0.1", "http://10.0.0.1:80/events")],
    )
    monkeypatch.setattr(requests, "get", lambda *a, **kw: _FakeResponse(401, []))

    with pytest.raises(WebServerLogsError, match="Authentication failed"):
        run_logs(["dev.local"], 80, "admin", "bad")


def test_run_logs_retries_on_transient_status(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """A transient non-200 (e.g. 503) is logged and the loop retries."""
    monkeypatch.setattr(
        web_server_logs,
        "_build_urls",
        lambda hosts, port: [("10.0.0.1", "http://10.0.0.1:80/events")],
    )
    monkeypatch.setattr(requests, "get", lambda *a, **kw: _FakeResponse(503, []))
    monkeypatch.setattr(
        web_server_logs.time,
        "sleep",
        lambda _d: (_ for _ in ()).throw(KeyboardInterrupt()),
    )

    with caplog.at_level(logging.ERROR):
        assert run_logs(["dev.local"], 80, None, None) == 0
    assert "Unexpected HTTP 503" in caplog.text


@pytest.mark.parametrize("status", (403, 404))
def test_run_logs_raises_on_permanent_status(
    monkeypatch: pytest.MonkeyPatch, status: int
) -> None:
    """A permanent 403/404 aborts instead of retrying the endpoint forever."""
    monkeypatch.setattr(
        web_server_logs,
        "_build_urls",
        lambda hosts, port: [("10.0.0.1", "http://10.0.0.1:80/events")],
    )
    monkeypatch.setattr(requests, "get", lambda *a, **kw: _FakeResponse(status, []))

    with pytest.raises(WebServerLogsError, match=str(status)):
        run_logs(["dev.local"], 80, None, None)


def test_run_logs_backs_off_on_repeated_failure(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Consecutive unreachable attempts grow the reconnect delay up to the cap."""
    monkeypatch.setattr(web_server_logs, "_build_urls", lambda hosts, port: [])
    delays: list[float] = []

    def record(delay: float) -> None:
        delays.append(delay)
        if len(delays) >= 4:
            raise KeyboardInterrupt

    monkeypatch.setattr(web_server_logs.time, "sleep", record)

    assert run_logs(["dev.local"], 80, None, None) == 0
    # 1 -> 2 -> 4 -> 8 ... doubling, capped at MAX_RECONNECT_DELAY (10.0).
    assert delays == [2.0, 4.0, 8.0, 10.0]


def test_run_logs_reports_unresolvable(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    """When no host resolves, an error is logged and the loop pauses/retries."""
    monkeypatch.setattr(web_server_logs, "_build_urls", lambda hosts, port: [])

    # Let the first reconnect pause pass so the loop continues, then interrupt
    # on the second so the retry path (the ``continue``) is exercised.
    sleeps = {"n": 0}

    def sleep(_delay: float) -> None:
        sleeps["n"] += 1
        if sleeps["n"] >= 2:
            raise KeyboardInterrupt

    monkeypatch.setattr(web_server_logs.time, "sleep", sleep)

    with caplog.at_level(logging.ERROR):
        assert run_logs(["dev.local"], 80, None, None) == 0
    assert sleeps["n"] == 2
    assert "Could not resolve" in caplog.text
