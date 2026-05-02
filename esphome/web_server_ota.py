"""HTTP-based OTA upload via the ``web_server`` component's ``/update`` endpoint.

This is the alternative to ``espota2`` (the native API OTA path). It is useful
when a device only has ``platform: web_server`` configured under ``ota:``, or
when the user has lost the native OTA password but still has ``web_server``
basic-auth credentials.
"""

from __future__ import annotations

import base64
from http.client import HTTPConnection, HTTPException, HTTPResponse
import logging
from pathlib import Path
import secrets
import socket
import sys
from typing import BinaryIO

from esphome.core import EsphomeError
from esphome.helpers import ProgressBar, resolve_ip_address

_LOGGER = logging.getLogger(__name__)

# Path on the device that accepts firmware uploads (see
# components/web_server/ota/ota_web_server.cpp -> canHandle()).
OTA_PATH = "/update"

# Read the firmware in 8 KiB chunks while streaming. Matches espota2's
# UPLOAD_BLOCK_SIZE so progress feedback feels similar between the two paths.
UPLOAD_BLOCK_SIZE = 8192

# Connect timeout when racing through resolved address candidates. Mirrors
# espota2 so a single unreachable address doesn't stall the whole run.
CONNECT_TIMEOUT = 20.0

# Per-request timeout once we are connected. The device reboots after a
# successful upload, so the response can take several seconds.
REQUEST_TIMEOUT = 120.0


class WebServerOTAError(EsphomeError):
    pass


def _build_multipart_envelope(filename: str) -> tuple[bytes, bytes, str]:
    """Return (prefix, suffix, boundary) for a single-file multipart body."""
    boundary = "esphomeOTA" + secrets.token_hex(16)
    prefix = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="update"; filename="{filename}"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode()
    suffix = f"\r\n--{boundary}--\r\n".encode()
    return prefix, suffix, boundary


def _send_request(
    sock: socket.socket,
    host: str,
    port: int,
    username: str | None,
    password: str | None,
    file_handle: BinaryIO,
    file_size: int,
    filename: str,
) -> HTTPResponse:
    """Send the OTA POST over an already-connected socket and return the response."""
    prefix, suffix, boundary = _build_multipart_envelope(filename)
    content_length = len(prefix) + file_size + len(suffix)

    conn = HTTPConnection(host, port, timeout=REQUEST_TIMEOUT)
    # Reuse the socket we already opened so error handling is uniform with
    # espota2 (one connect attempt per resolved address).
    conn.sock = sock

    conn.putrequest("POST", OTA_PATH, skip_host=False, skip_accept_encoding=True)
    conn.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
    conn.putheader("Content-Length", str(content_length))
    conn.putheader("Connection", "close")
    if username is not None and password is not None:
        token = base64.b64encode(f"{username}:{password}".encode()).decode()
        conn.putheader("Authorization", f"Basic {token}")
    conn.endheaders()

    sock.sendall(prefix)

    progress = ProgressBar()
    sent = 0
    while True:
        chunk = file_handle.read(UPLOAD_BLOCK_SIZE)
        if not chunk:
            break
        try:
            sock.sendall(chunk)
        except OSError as err:
            sys.stderr.write("\n")
            raise WebServerOTAError(f"Error sending firmware data: {err}") from err
        sent += len(chunk)
        progress.update(sent / file_size)
    progress.done()

    sock.sendall(suffix)

    return conn.getresponse()


def _try_upload(
    host: str,
    port: int,
    username: str | None,
    password: str | None,
    filename: Path,
) -> tuple[int, str | None]:
    from esphome.core import CORE

    try:
        addr_infos = resolve_ip_address(host, port, address_cache=CORE.address_cache)
    except EsphomeError as err:
        _LOGGER.error(
            "Error resolving IP address of %s. Is it connected to WiFi?", host
        )
        if not CORE.dashboard:
            _LOGGER.error("(If you know the IP, try --device <IP>)")
        raise WebServerOTAError(err) from err

    file_size = filename.stat().st_size
    _LOGGER.info("Uploading %s (%s bytes) via web_server", filename, file_size)

    last_error: str | None = None
    for af, socktype, _, _, sa in addr_infos:
        _LOGGER.info("Connecting to %s port %s...", sa[0], sa[1])
        sock = socket.socket(af, socktype)
        sock.settimeout(CONNECT_TIMEOUT)
        try:
            sock.connect(sa)
        except OSError as err:
            sock.close()
            _LOGGER.error("Connecting to %s port %s failed: %s", sa[0], sa[1], err)
            last_error = str(err)
            continue

        sock.settimeout(REQUEST_TIMEOUT)
        _LOGGER.info("Connected to %s", sa[0])
        try:
            with open(filename, "rb") as file_handle:
                response = _send_request(
                    sock,
                    host,
                    port,
                    username,
                    password,
                    file_handle,
                    file_size,
                    filename.name,
                )
                body = response.read().decode("utf-8", errors="replace").strip()
        except (OSError, HTTPException) as err:
            _LOGGER.error("OTA upload to %s failed: %s", sa[0], err)
            last_error = str(err)
            continue
        finally:
            sock.close()

        if response.status == 401:
            raise WebServerOTAError(
                "Authentication failed (HTTP 401). Check the 'web_server' "
                "'auth' username and password."
            )
        if response.status != 200:
            raise WebServerOTAError(
                f"Unexpected HTTP {response.status} response from device: {body}"
            )

        # The endpoint returns HTTP 200 in both success and failure paths;
        # the body is what tells us which one. See
        # components/web_server/ota/ota_web_server.cpp -> handleRequest().
        if "Successful" in body:
            _LOGGER.info("OTA successful")
            return 0, sa[0]

        raise WebServerOTAError(
            f"Device reported OTA failure: {body or 'no response body'}"
        )

    if last_error is None:
        _LOGGER.error("Could not resolve %s", host)
    else:
        _LOGGER.error("Connection failed: %s", last_error)
    return 1, None


def run_ota(
    remote_hosts: str | list[str],
    remote_port: int,
    username: str | None,
    password: str | None,
    filename: Path,
) -> tuple[int, str | None]:
    """Upload ``filename`` to the first reachable host via ``web_server`` OTA.

    Mirrors :func:`esphome.espota2.run_ota` so callers can swap between the
    two paths with the same return contract: ``(0, host)`` on success or
    ``(1, None)`` on failure.
    """
    hosts = [remote_hosts] if isinstance(remote_hosts, str) else list(remote_hosts)
    if not hosts:
        _LOGGER.error("No hosts to upload to.")
        return 1, None

    last_error: WebServerOTAError | None = None
    for host in hosts:
        try:
            exit_code, used_host = _try_upload(
                host, remote_port, username, password, filename
            )
        except WebServerOTAError as err:
            _LOGGER.error("%s", err)
            last_error = err
            continue
        if exit_code == 0:
            return 0, used_host

    if last_error is not None:
        return 1, None
    _LOGGER.error("All hosts failed.")
    return 1, None
