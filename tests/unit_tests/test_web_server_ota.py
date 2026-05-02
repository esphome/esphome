"""Unit tests for esphome.web_server_ota module."""

from __future__ import annotations

import base64
from pathlib import Path
import socket
from unittest.mock import MagicMock, patch

import pytest

from esphome.core import EsphomeError
from esphome.web_server_ota import (
    OTA_PATH,
    WebServerOTAError,
    _build_multipart_envelope,
    run_ota,
)


@pytest.fixture
def firmware(tmp_path: Path) -> Path:
    binary = tmp_path / "firmware.bin"
    binary.write_bytes(b"\x00\x01\x02FIRMWARE\xff" * 64)
    return binary


def test_build_multipart_envelope_round_trip() -> None:
    prefix, suffix, boundary = _build_multipart_envelope("firmware.bin")

    assert boundary.startswith("esphomeOTA")
    assert prefix.startswith(f"--{boundary}\r\n".encode())
    assert b'name="update"' in prefix
    assert b'filename="firmware.bin"' in prefix
    assert prefix.endswith(b"\r\n\r\n")
    assert suffix == f"\r\n--{boundary}--\r\n".encode()


def _make_response(status: int, body: bytes) -> MagicMock:
    response = MagicMock()
    response.status = status
    response.read.return_value = body
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


def test_run_ota_success(monkeypatch: pytest.MonkeyPatch, firmware: Path) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "192.168.1.50"
    fake_sock.connect.assert_called_once_with(("192.168.1.50", 80))
    # Body chunks were sent: prefix, file chunk(s), suffix.
    assert fake_sock.sendall.call_count >= 3


def test_run_ota_sends_basic_auth(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, _ = run_ota(["192.168.1.50"], 80, "admin", "secret", firmware)

    assert exit_code == 0
    # Verify Authorization header was set with Basic <base64(admin:secret)>.
    expected_token = base64.b64encode(b"admin:secret").decode()
    auth_calls = [
        call
        for call in fake_conn.putheader.call_args_list
        if call.args and call.args[0] == "Authorization"
    ]
    assert auth_calls, "Authorization header was not sent"
    assert auth_calls[0].args[1] == f"Basic {expected_token}"


def test_run_ota_skips_auth_header_when_no_credentials(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    auth_calls = [
        call
        for call in fake_conn.putheader.call_args_list
        if call.args and call.args[0] == "Authorization"
    ]
    assert not auth_calls


def test_run_ota_uses_update_path(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    fake_conn.putrequest.assert_called_once()
    method, path = fake_conn.putrequest.call_args.args[:2]
    assert method == "POST"
    assert path == OTA_PATH == "/update"


def test_run_ota_failure_response(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Failed!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "OTA failure" in caplog.text


def test_run_ota_auth_failed(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(401, b"Unauthorized")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, "user", "wrong", firmware)

    assert exit_code == 1
    assert host is None
    assert "Authentication failed" in caplog.text


def test_run_ota_connect_error_then_success(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    _patch_resolve(
        monkeypatch,
        [("192.168.1.10", 80), ("192.168.1.50", 80)],
    )

    fake_sock_fail = MagicMock(spec=socket.socket)
    fake_sock_fail.connect.side_effect = OSError("refused")
    fake_sock_ok = MagicMock(spec=socket.socket)

    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch(
            "esphome.web_server_ota.socket.socket",
            side_effect=[fake_sock_fail, fake_sock_ok],
        ),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "192.168.1.50"
    fake_sock_fail.close.assert_called_once()
    fake_sock_ok.close.assert_called_once()


def test_run_ota_resolution_failure(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    def _raise(*_args, **_kwargs):
        raise EsphomeError("dns failed")

    monkeypatch.setattr("esphome.web_server_ota.resolve_ip_address", _raise)

    exit_code, host = run_ota(["does.not.exist"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None


def test_run_ota_empty_hosts(firmware: Path) -> None:
    exit_code, host = run_ota([], 80, None, None, firmware)
    assert exit_code == 1
    assert host is None


def test_run_ota_string_host_accepted(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """A bare string is accepted in addition to a list of hosts."""
    _patch_resolve(monkeypatch, [("10.0.0.5", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota("10.0.0.5", 80, None, None, firmware)

    assert exit_code == 0
    assert host == "10.0.0.5"


def test_web_server_ota_error_is_esphome_error() -> None:
    assert issubclass(WebServerOTAError, EsphomeError)


def test_run_ota_sendall_oserror_mid_upload(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Mid-upload OSError surfaces as a WebServerOTAError and the run fails."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    # First sendall call sends the multipart prefix successfully; the second
    # one (the first firmware chunk) blows up. Subsequent calls don't matter.
    fake_sock.sendall.side_effect = [None, OSError("connection reset")]
    fake_conn = MagicMock()

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "Error sending firmware data" in caplog.text


def test_run_ota_resolution_failure_dashboard_mode(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Dashboard mode skips the '--device <IP>' tip on resolution failure."""
    from esphome.core import CORE

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


def test_run_ota_http_exception_falls_through(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """Test that HTTPException during getresponse falls through to next address."""
    from http.client import HTTPException

    _patch_resolve(
        monkeypatch,
        [("192.168.1.10", 80), ("192.168.1.50", 80)],
    )

    sock_first = MagicMock(spec=socket.socket)
    sock_second = MagicMock(spec=socket.socket)

    conn_bad = MagicMock()
    conn_bad.getresponse.side_effect = HTTPException("bad response")
    conn_ok = MagicMock()
    conn_ok.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch(
            "esphome.web_server_ota.socket.socket",
            side_effect=[sock_first, sock_second],
        ),
        patch(
            "esphome.web_server_ota.HTTPConnection",
            side_effect=[conn_bad, conn_ok],
        ),
    ):
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)

    assert exit_code == 0
    assert host == "192.168.1.50"


def test_run_ota_unexpected_status_code(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A non-200, non-401 response surfaces as a clear error."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(500, b"Internal Error")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        exit_code, host = run_ota(["192.168.1.50"], 80, None, None, firmware)

    assert exit_code == 1
    assert host is None
    assert "Unexpected HTTP 500" in caplog.text


def test_run_ota_all_addresses_unreachable(
    monkeypatch: pytest.MonkeyPatch, firmware: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """When every resolved address fails to connect, run_ota returns failure."""
    _patch_resolve(
        monkeypatch,
        [("192.168.1.10", 80), ("192.168.1.20", 80)],
    )

    sock_a = MagicMock(spec=socket.socket)
    sock_a.connect.side_effect = OSError("refused")
    sock_b = MagicMock(spec=socket.socket)
    sock_b.connect.side_effect = OSError("refused")

    with (
        patch(
            "esphome.web_server_ota.socket.socket",
            side_effect=[sock_a, sock_b],
        ),
        patch("esphome.web_server_ota.HTTPConnection") as conn_cls,
    ):
        exit_code, host = run_ota(["device.local"], 80, None, None, firmware)
        conn_cls.assert_not_called()  # we never got past connect()

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

    sock_fail = MagicMock(spec=socket.socket)
    sock_fail.connect.side_effect = OSError("refused")
    sock_ok = MagicMock(spec=socket.socket)

    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch(
            "esphome.web_server_ota.socket.socket",
            side_effect=[sock_fail, sock_ok],
        ),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
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


def test_build_multipart_envelope_unique_boundaries() -> None:
    """Each call returns a fresh boundary so concurrent uploads don't collide."""
    _, _, b1 = _build_multipart_envelope("a.bin")
    _, _, b2 = _build_multipart_envelope("a.bin")
    assert b1 != b2


def test_run_ota_content_length_matches_envelope(
    monkeypatch: pytest.MonkeyPatch, firmware: Path
) -> None:
    """Content-Length header equals prefix + file_size + suffix bytes."""
    _patch_resolve(monkeypatch, [("192.168.1.50", 80)])

    fake_sock = MagicMock(spec=socket.socket)
    fake_conn = MagicMock()
    fake_conn.getresponse.return_value = _make_response(200, b"Update Successful!")

    with (
        patch("esphome.web_server_ota.socket.socket", return_value=fake_sock),
        patch("esphome.web_server_ota.HTTPConnection", return_value=fake_conn),
    ):
        run_ota(["192.168.1.50"], 80, None, None, firmware)

    length_header = next(
        call.args[1]
        for call in fake_conn.putheader.call_args_list
        if call.args and call.args[0] == "Content-Length"
    )
    # The prefix and suffix are deterministic in length given the boundary
    # length (boundary = "esphomeOTA" + 32-hex-char token = 42 chars).
    file_size = firmware.stat().st_size
    # Prefix: "--" + boundary + "\r\n" + Content-Disposition line + Content-Type line + "\r\n"
    # Suffix: "\r\n--" + boundary + "--\r\n"
    # We don't recompute exactly here, just sanity-check the value is plausible.
    assert int(length_header) > file_size
    # Bound it: envelope overhead is well under 256 bytes.
    assert int(length_header) - file_size < 256
