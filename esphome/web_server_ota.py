"""HTTP-based OTA upload via the ``web_server`` component's ``/update`` endpoint.

This is the alternative to ``espota2`` (the native API OTA path). It is useful
when a device only has ``platform: web_server`` configured under ``ota:``, or
when the user has lost the native OTA password but still has ``web_server``
basic-auth credentials.
"""

from __future__ import annotations

import logging
from pathlib import Path
import secrets
import socket
from typing import BinaryIO

import requests
from requests.auth import HTTPBasicAuth

from esphome.core import EsphomeError
from esphome.helpers import ProgressBar, resolve_ip_address

_LOGGER = logging.getLogger(__name__)

# Path on the device that accepts firmware uploads (see
# components/web_server/ota/ota_web_server.cpp -> canHandle()).
OTA_PATH = "/update"

# Form field name for the firmware file in the multipart body. AsyncWebServer
# treats any uploaded file the same regardless of name, but use a stable name
# so the curl-equivalent ``-F "update=@firmware.bin"`` matches.
FORM_FIELD = "update"

# (connect_timeout, read_timeout). The device reboots after a successful
# upload, so the read side must allow for a slow flash + response.
TIMEOUT = (20.0, 120.0)

# Read the firmware in 8 KiB chunks while streaming, matching urllib3's
# default block size and espota2's UPLOAD_BLOCK_SIZE.
UPLOAD_BLOCK_SIZE = 8192


class WebServerOTAError(EsphomeError):
    pass


class _MultipartStreamer:
    """Streams a single-file multipart/form-data body during transmission.

    ``requests.post(files=...)`` materializes the entire multipart body in
    memory before sending, which makes any progress callback wired into the
    file-like fire instantly during encoding rather than during the actual
    network send. By exposing this class via ``data=streamer`` (with
    ``__len__`` so urllib3 sets ``Content-Length`` instead of chunked
    transfer encoding), the body is read in 8 KiB chunks during the POST and
    the progress bar tracks bytes that have left the host.
    """

    def __init__(self, file_handle: BinaryIO, file_size: int, filename: str) -> None:
        self.boundary = "esphomeOTA" + secrets.token_hex(16)
        self._prefix = (
            f"--{self.boundary}\r\n"
            f'Content-Disposition: form-data; name="{FORM_FIELD}"; '
            f'filename="{filename}"\r\n'
            f"Content-Type: application/octet-stream\r\n\r\n"
        ).encode()
        self._suffix = f"\r\n--{self.boundary}--\r\n".encode()
        self._file = file_handle
        self._file_size = file_size
        self._total = len(self._prefix) + file_size + len(self._suffix)
        self._sent = 0
        # Pending bytes from the prefix/suffix segments; each is consumed
        # before reading the next segment.
        self._head = self._prefix
        self._tail = self._suffix
        self._file_done = False
        self.progress = ProgressBar()

    def __len__(self) -> int:
        return self._total

    @property
    def content_type(self) -> str:
        return f"multipart/form-data; boundary={self.boundary}"

    def read(self, size: int = -1) -> bytes:
        # urllib3 calls read(blocksize) repeatedly until it returns b''.
        # Walk the prefix -> file -> suffix segments in order, accumulating
        # up to ``size`` bytes across segment boundaries so a single
        # ``read(-1)`` returns the whole body and a chunked ``read(8192)``
        # ticks progress for every block.
        remaining = self._total if size is None or size < 0 else size
        if remaining == 0:
            return b""

        out = bytearray()
        while remaining > 0:
            if self._head:
                take = min(remaining, len(self._head))
                out += self._head[:take]
                self._head = self._head[take:]
                remaining -= take
                continue
            if not self._file_done:
                chunk = self._file.read(remaining)
                if not chunk:
                    self._file_done = True
                    continue
                out += chunk
                remaining -= len(chunk)
                continue
            if self._tail:
                take = min(remaining, len(self._tail))
                out += self._tail[:take]
                self._tail = self._tail[take:]
                remaining -= take
                continue
            break  # nothing left to emit

        if out:
            # ``self._total`` is always > 0 since the prefix and suffix
            # contribute well over a hundred bytes regardless of file size.
            self._sent += len(out)
            self.progress.update(self._sent / self._total)
        return bytes(out)


def _format_url(af: int, sa: tuple, port: int) -> str:
    """Build an HTTP URL for the given resolved sockaddr.

    Wraps IPv6 literals in brackets and includes the percent-encoded zone
    index for link-local addresses (RFC 6874: literal ``%`` -> ``%25``).
    """
    ip = sa[0]
    if af == socket.AF_INET6:
        scope_id = sa[3] if len(sa) >= 4 else 0
        host_part = f"[{ip}%25{scope_id}]" if scope_id else f"[{ip}]"
    else:
        host_part = ip
    return f"http://{host_part}:{port}{OTA_PATH}"


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
    _LOGGER.info("Uploading %s (%s bytes) via web_server OTA", filename, file_size)

    auth = HTTPBasicAuth(username, password) if username and password else None

    last_error: str | None = None
    # Each entry is the resolved IP for ``host``; iterate in order so IPv4
    # and IPv6 candidates are both tried (mirrors espota2's behavior).
    for af, _socktype, _, _, sa in addr_infos:
        ip = sa[0]
        url = _format_url(af, sa, port)
        _LOGGER.info("Connecting to %s port %s...", ip, port)

        try:
            with open(filename, "rb") as file_handle:
                streamer = _MultipartStreamer(file_handle, file_size, filename.name)
                headers = {
                    "Content-Type": streamer.content_type,
                    "Connection": "close",
                }
                try:
                    response = requests.post(
                        url,
                        data=streamer,
                        auth=auth,
                        timeout=TIMEOUT,
                        headers=headers,
                    )
                finally:
                    # Always finalize the progress bar; on a transport error
                    # the streamer may not have reached 100%.
                    streamer.progress.done()
        except requests.ConnectionError as err:
            _LOGGER.error("Connecting to %s port %s failed: %s", ip, port, err)
            last_error = str(err)
            continue
        except requests.RequestException as err:
            _LOGGER.error("OTA upload to %s failed: %s", ip, err)
            last_error = str(err)
            continue

        if response.status_code == 401:
            raise WebServerOTAError(
                "Authentication failed (HTTP 401). Check the 'web_server' "
                "'auth' username and password."
            )
        if response.status_code != 200:
            body = response.text.strip()
            detail = body or response.reason or "no response body"
            raise WebServerOTAError(
                f"Unexpected HTTP {response.status_code} response from device: {detail}"
            )

        # The endpoint returns HTTP 200 in both success and failure paths;
        # the body is what tells us which one. See
        # components/web_server/ota/ota_web_server.cpp -> handleRequest().
        body = response.text.strip()
        if "Successful" in body:
            # Surface the device's exact response so the user can see the
            # firmware was accepted (not just that the HTTP request returned
            # 200) and that the device intends to reboot.
            _LOGGER.info("Device response: %s", body)
            _LOGGER.info("OTA successful")
            return 0, ip

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
