"""Unit tests for esphome.web_server_ota module."""

from __future__ import annotations

import io
import logging
from pathlib import Path
import socket
from unittest.mock import MagicMock, patch

import pytest
import requests
from requests.auth import HTTPBasicAuth

from esphome.core import CORE, EsphomeError
from esphome.helpers import ProgressBar
from esphome.web_server_ota import (
    OTA_PATH,
    WebServerOTAError,
    _MultipartStreamer,
    run_ota,
)


@pytest.fixture
def firmware(tmp_path: Path) -> Path:
    binary = tmp_path / "firmware.bin"
    binary.write_bytes(b"\x00\x01\x02FIRMWARE\xff" * 64)
    return binary


def _make_response(status: int, body: str) -> MagicMock:
    response = MagicMock(spec=requests.Response)
    response.status_code = status
    response.text = body
    response.reason = ""
    return response


def _patch_resolve(
    monkeypatch: pytest.MonkeyPatch, hosts: list[tuple[str, int]]
) -> None:
    """Replace resolve_ip_address so tests don't actually do DNS."""
    addr_infos = [
        (socket.AF_INET, socket.SOCK_STREAM, 0, "", (host, port))
        for host, port in hosts
    ]
    monkeypatch.setattr(
        "esphome.web_server_ota.resolve_ip_address", lambda *a, **kw: addr_infos
    )


# ---------------------------------------------------------------------------
# _MultipartStreamer
# ---------------------------------------------------------------------------


def test_multipart_streamer_emits_full_body() -> None:
    """Streaming the whole body in one call yields prefix + file + suffix."""
    data = b"abcdef" * 100
    streamer = _MultipartStreamer(io.BytesIO(data), len(data), "fw.bin")

    body = streamer.read()
    while True:
        chunk = streamer.read()
        if not chunk:
            break
        body += chunk

    assert body.startswith(f"--{streamer.boundary}\r\n".encode())
    assert b'name="update"' in body
    assert b'filename="fw.bin"' in body
    assert data in body
    assert body.endswith(f"\r\n--{streamer.boundary}--\r\n".encode())


def test_multipart_streamer_chunked_read_matches_full_read() -> None:
    """Chunked reads (urllib3 calls read(8192) repeatedly) yield the same body."""
    data = b"abcdef" * 1000  # 6000 bytes
    full = _MultipartStreamer(io.BytesIO(data), len(data), "fw.bin").read()

    streamed = bytearray()
    s = _MultipartStreamer(io.BytesIO(data), len(data), "fw.bin")
    # Same boundary lengths -> identical total length.
    while True:
        chunk = s.read(64)
        if not chunk:
            break
        streamed += chunk
    # Boundaries are random per instance, so compare lengths and structure.
    assert len(streamed) == len(full)
    assert streamed.startswith(f"--{s.boundary}\r\n".encode())
    assert streamed.endswith(f"\r\n--{s.boundary}--\r\n".encode())


def test_multipart_streamer_len_matches_emitted_bytes() -> None:
    """``__len__`` is what urllib3 uses to set Content-Length, so it must
    equal the total bytes emitted by ``read``."""
    data = b"x" * 12345
    s = _MultipartStreamer(io.BytesIO(data), len(data), "fw.bin")
    declared = len(s)

    emitted = 0
    while True:
        chunk = s.read(1024)
        if not chunk:
            break
        emitted += len(chunk)

    assert emitted == declared


def test_multipart_streamer_progress_ticks_during_read() -> None:
    """Each read advances the progress bar (this is the whole point of
    streaming via ``data=``: progress reflects bytes leaving the host)."""
    data = b"x" * 1000
    s = _MultipartStreamer(io.BytesIO(data), len(data), "fw.bin")

    updates: list[float] = []
    s.progress.update = updates.append  # type: ignore[method-assign]

    while True:
        chunk = s.read(128)
        if not chunk:
            break

    assert updates, "progress.update was never called"
    # Strictly non-decreasing.
    assert updates == sorted(updates)
    # Final update reaches (within FP) 1.0 because all bytes were read.
    assert updates[-1] == pytest.approx(1.0, abs=1e-9)


def test_multipart_streamer_content_type_includes_boundary() -> None:
    s = _MultipartStreamer(io.BytesIO(b""), 0, "fw.bin")
    assert s.content_type == f"multipart/form-data; boundary={s.boundary}"


def test_multipart_streamer_zero_size_file() -> None:
    """A zero-byte file still produces a well-formed body and progress is
    skipped (avoiding a divide-by-zero on the empty file segment)."""
    s = _MultipartStreamer(io.BytesIO(b""), 0, "empty.bin")
    body = b""
    while True:
        chunk = s.read(64)
        if not chunk:
            break
        body += chunk
    assert body.startswith(f"--{s.boundary}".encode())
    assert body.endswith(f"--{s.boundary}--\r\n".encode())


def test_multipart_streamer_unique_boundary_per_instance() -> None:
    a = _MultipartStreamer(io.BytesIO(b""), 0, "a")
    b = _MultipartStreamer(io.BytesIO(b""), 0, "a")
    assert a.boundary != b.boundary


def test_multipart_streamer_zero_size_read_returns_empty() -> None:
    """``read(0)`` short-circuits without touching state."""
    s = _MultipartStreamer(io.BytesIO(b"x" * 10), 10, "fw.bin")
    assert s.read(0) == b""
    # No bytes consumed.
    assert s._sent == 0


# ---------------------------------------------------------------------------
# run_ota
# ---------------------------------------------------------------------------


def test_run_ota_success(monkeypatch: pytest.MonkeyPatch, firmware: Path) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "192.168.1.50"
    post.assert_called_once()
    args, kwargs = post.call_args
    assert args == (f"http://192.168.1.50:80{OTA_PATH}",)
    assert kwargs["auth"] is None
    # Streaming body, not files=, so progress fires during transmission.
    assert "files" not in kwargs
    assert isinstance(kwargs["data"], _MultipartStreamer)
    assert kwargs["headers"]["Content-Type"] == kwargs["data"].content_type
    assert kwargs["headers"]["Connection"] == "close"


def test_run_ota_logs_device_response_body(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """The device's HTTP response body is surfaced on success."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])
    caplog.set_level(logging.INFO, logger="esphome.web_server_ota")

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert "Device response: Update Successful!" in caplog.text
    assert "OTA successful" in caplog.text


def test_run_ota_log_says_via_web_server(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """The upload-start log line names the transport explicitly."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])
    caplog.set_level(logging.INFO, logger="esphome.web_server_ota")

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert "via web_server OTA" in caplog.text


def test_run_ota_sends_basic_auth(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        exit_code, _ = run_ota(["192.168.1.50"], 80, "admin", "secret", firmware)

    assert exit_code == 0
    auth = post.call_args.kwargs["auth"]
    assert isinstance(auth, HTTPBasicAuth)
    assert auth.username == "admin"
    assert auth.password == "secret"


def test_run_ota_skips_auth_when_no_credentials(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert post.call_args.kwargs["auth"] is None


def test_run_ota_skips_auth_when_only_username(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """Both username and password are required to send Basic auth."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        run_ota(["192.168.1.50"], 80, "admin", None, firmware)

    assert post.call_args.kwargs["auth"] is None


def test_run_ota_uses_update_url(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 8080)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        run_ota(["192.168.1.50"], 8080, None, None, firmware)

    url = post.call_args.args[0]
    assert url == f"http://192.168.1.50:8080{OTA_PATH}"
    assert OTA_PATH == "/update"


def test_run_ota_failure_response(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Failed!"),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "OTA failure" in caplog.text


def test_run_ota_failure_response_empty_body(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, ""),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "no response body" in caplog.text


def test_run_ota_auth_failed(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(401, "Unauthorized"),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, "user", "wrong", firmware)

    assert exit_code == 1
    assert host is None
    assert "Authentication failed" in caplog.text


def test_run_ota_unexpected_status_code(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(500, "Internal Error"),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "Unexpected HTTP 500" in caplog.text


def test_run_ota_unexpected_status_empty_body_falls_back(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Empty response body uses response.reason / a fallback in the error."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    response = _make_response(503, "")
    response.reason = "Service Unavailable"

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=response,
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "Service Unavailable" in caplog.text


def test_run_ota_unexpected_status_no_body_no_reason(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Empty body and empty reason still produce a usable error message."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    response = _make_response(599, "")
    response.reason = ""

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=response,
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert "no response body" in caplog.text


def test_run_ota_connection_error_then_success(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """First resolved address fails to connect, second succeeds."""
    _patch_resolve(
        monkeypatch,
        [("192.168.1.10", 80), ("192.168.1.50", 80)],
    )

    with patch(
        "esphome.web_server_ota.requests.post",
        side_effect=[
            requests.ConnectionError("refused"),
            _make_response(200, "Update Successful!"),
        ],
    ) as post:
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "192.168.1.50"
    assert post.call_count == 2


def test_run_ota_request_exception_falls_through(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """A non-ConnectionError RequestException (e.g. timeout) falls through too."""
    _patch_resolve(
        monkeypatch,
        [("192.168.1.10", 80), ("192.168.1.50", 80)],
    )

    with patch(
        "esphome.web_server_ota.requests.post",
        side_effect=[
            requests.Timeout("read timeout"),
            _make_response(200, "Update Successful!"),
        ],
    ):
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "192.168.1.50"


def test_run_ota_all_addresses_unreachable(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """When every resolved address fails to connect, run_ota returns failure."""
    _patch_resolve(
        monkeypatch,
        [("192.168.1.10", 80), ("192.168.1.20", 80)],
    )

    with patch(
        "esphome.web_server_ota.requests.post",
        side_effect=requests.ConnectionError("refused"),
    ):
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    # Per-address failure is logged for each attempt; final summary follows.
    assert caplog.text.count("OTA upload to ") >= 2
    assert "OTA upload failed." in caplog.text


def test_run_ota_no_resolved_addresses(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """If resolve_ip_address returns no candidates, log and return failure."""
    _patch_resolve(monkeypatch, [])

    exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "Could not resolve 192.168.1.50" in caplog.text


def test_run_ota_resolution_failure(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    def _raise(*_args, **_kwargs):
        raise EsphomeError("dns failed")

    monkeypatch.setattr("esphome.web_server_ota.resolve_ip_address", _raise)

    exit_code, host = run_ota(["does.not.exist"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None


def test_run_ota_resolution_failure_dashboard_mode(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Dashboard mode skips the '--device <IP>' tip on resolution failure."""

    def _raise(*_args, **_kwargs):
        raise EsphomeError("dns failed")

    monkeypatch.setattr("esphome.web_server_ota.resolve_ip_address", _raise)
    monkeypatch.setattr(CORE, "dashboard", True)
    try:
        exit_code, host = run_ota(["does.not.exist"], 80, None, None, firmware)
    finally:
        monkeypatch.setattr(CORE, "dashboard", False)

    assert exit_code == 1
    assert host is None
    assert "--device <IP>" not in caplog.text


def test_run_ota_empty_hosts(firmware: Path) -> None:
    exit_code, host = run_ota([], 80, None, None, firmware)
    assert exit_code == 1
    assert host is None


def test_run_ota_string_host_accepted(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """A bare string is accepted in addition to a list of hosts."""
    _patch_resolve(monkeypatch, [("10.0.0.5", 80)])

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ):
        exit_code, host = run_ota("10.0.0.5", 80, None, None, firmware)

    assert exit_code == 0
    assert host == "10.0.0.5"


def test_run_ota_multiple_hosts_first_fails(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """Multi-host fallthrough: first host's addresses all fail, second host wins."""
    addr_lookup = {
        "primary.local": [
            (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.10", 80)),
        ],
        "secondary.local": [
            (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.50", 80)),
        ],
    }

    def _resolve(host, port, address_cache=None):  # noqa: ARG001
        return addr_lookup[host]

    monkeypatch.setattr("esphome.web_server_ota.resolve_ip_address", _resolve)

    with patch(
        "esphome.web_server_ota.requests.post",
        side_effect=[
            requests.ConnectionError("refused"),
            _make_response(200, "Update Successful!"),
        ],
    ):
        exit_code, host = run_ota(
            ["primary.local", "secondary.local"], 80, None, None, firmware
        )

    assert exit_code == 0
    assert host == "192.168.1.50"


def test_run_ota_all_hosts_return_failure_no_exception(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """All hosts resolve to no addresses; run_ota cleanly returns failure."""
    addr_lookup = {
        "a.local": [],
        "b.local": [],
    }

    def _resolve(host, port, address_cache=None):  # noqa: ARG001
        return addr_lookup[host]

    monkeypatch.setattr("esphome.web_server_ota.resolve_ip_address", _resolve)

    exit_code, host = run_ota(["a.local", "b.local"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    # Each host gets its own "Could not resolve" log line + final summary.
    assert caplog.text.count("Could not resolve") == 2
    assert "OTA upload failed." in caplog.text


def test_web_server_ota_error_is_esphome_error() -> None:
    assert issubclass(WebServerOTAError, EsphomeError)


def test_run_ota_finalizes_progress_bar_on_success(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """progress.done() fires on the success path (finally block)."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    done_called: list[bool] = []

    with (
        patch(
            "esphome.web_server_ota.requests.post",
            return_value=_make_response(200, "Update Successful!"),
        ),
        patch.object(ProgressBar, "done", lambda self: done_called.append(True)),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert done_called


def test_run_ota_finalizes_progress_bar_on_failure(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """progress.done() fires when the request itself raises (finally block)."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    done_called: list[bool] = []

    with (
        patch(
            "esphome.web_server_ota.requests.post",
            side_effect=requests.ConnectionError("boom"),
        ),
        patch.object(ProgressBar, "done", lambda self: done_called.append(True)),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert done_called


def test_run_ota_ipv6_url_brackets_host(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """IPv6 candidates are bracketed in the URL so the port parses correctly."""
    addr_infos = [
        (socket.AF_INET6, socket.SOCK_STREAM, 0, "", ("2001:db8::1", 80, 0, 0)),
    ]
    monkeypatch.setattr(
        "esphome.web_server_ota.resolve_ip_address", lambda *a, **kw: addr_infos
    )

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "2001:db8::1"
    url = post.call_args.args[0]
    assert url == f"http://[2001:db8::1]:80{OTA_PATH}"


def test_run_ota_ipv6_link_local_includes_scope_id(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """Link-local IPv6 candidates include the percent-encoded zone index."""
    addr_infos = [
        (socket.AF_INET6, socket.SOCK_STREAM, 0, "", ("fe80::1", 80, 0, 3)),
    ]
    monkeypatch.setattr(
        "esphome.web_server_ota.resolve_ip_address", lambda *a, **kw: addr_infos
    )

    with patch(
        "esphome.web_server_ota.requests.post",
        return_value=_make_response(200, "Update Successful!"),
    ) as post:
        exit_code, _ = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    url = post.call_args.args[0]
    assert url == f"http://[fe80::1%253]:80{OTA_PATH}"
