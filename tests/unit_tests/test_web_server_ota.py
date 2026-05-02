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
