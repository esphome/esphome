"""Unit tests for esphome.web_server_ota module."""

from __future__ import annotations

import io
from pathlib import Path
import socket
from unittest.mock import MagicMock, patch

import pytest
import requests
from requests.auth import HTTPBasicAuth

from esphome.core import CORE, EsphomeError
from esphome.web_server_ota import (
    FORM_FIELD,
    OTA_PATH,
    WebServerOTAError,
    _ProgressFile,
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


def test_progress_file_reports_progress() -> None:
    """_ProgressFile forwards reads and tracks consumption ratio."""
    data = b"abcdef" * 100  # 600 bytes
    pf = _ProgressFile(io.BytesIO(data), len(data))

    chunk = pf.read(200)
    assert chunk == data[:200]
    assert pf._read == 200

    rest = pf.read()
    assert rest == data[200:]
    # __len__ exposes the total size to urllib3 streaming detection.
    assert len(pf) == len(data)

    # A trailing zero-byte read after consuming the whole file finalizes the
    # progress bar (urllib3 issues this read to detect EOF).
    eof = pf.read()
    assert eof == b""


def test_progress_file_zero_size_skips_progress() -> None:
    """A zero-length size shouldn't divide-by-zero in the progress update."""
    # Pass a non-empty BytesIO but report size=0 so the chunk-truthy branch
    # exercises the ``self._size > 0`` guard.
    pf = _ProgressFile(io.BytesIO(b"x"), 0)
    assert pf.read() == b"x"
    assert len(pf) == 0


def test_progress_file_short_read_does_not_finalize() -> None:
    """Reaching EOF before reading all promised bytes leaves progress open."""
    # File has only 5 bytes but we claim 100.
    pf = _ProgressFile(io.BytesIO(b"short"), 100)
    # Drain the file.
    assert pf.read() == b"short"
    # Subsequent read returns empty; _read (5) < _size (100) so the
    # finalize branch is intentionally skipped.
    assert pf.read() == b""


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
    assert FORM_FIELD in kwargs["files"]
    file_tuple = kwargs["files"][FORM_FIELD]
    assert file_tuple[0] == firmware.name
    assert file_tuple[2] == "application/octet-stream"


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
    """An empty body falls back to a deterministic error message."""
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
    assert "Connection failed" in caplog.text


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
    """All hosts return (1, None) without raising; final fallthrough hits."""
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
    assert "All hosts failed" in caplog.text


def test_web_server_ota_error_is_esphome_error() -> None:
    assert issubclass(WebServerOTAError, EsphomeError)
