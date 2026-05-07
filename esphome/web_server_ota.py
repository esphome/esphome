"""HTTP-based OTA upload via the ``web_server`` component's ``/update`` endpoint.

This is the alternative to ``espota2`` (the native API OTA path). Useful when
a device only has ``platform: web_server`` configured under ``ota:``, or when
the user has lost the native OTA password but still has ``web_server`` basic
auth credentials.
"""

from __future__ import annotations

import io
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

OTA_PATH = "/update"
FORM_FIELD = "update"
# (connect_timeout, read_timeout). The device reboots after a successful
# upload so the read side must allow for a slow flash + response.
TIMEOUT = (20.0, 120.0)


class WebServerOTAError(EsphomeError):
    pass


class _MultipartStreamer:
    """Stream a single-file multipart/form-data body during transmission.

    ``requests.post(files=...)`` materializes the entire body in memory before
    sending, so a progress callback wired into the file-like fires during
    encoding instead of during the network send. Pass this via ``data=``
    (with ``__len__`` so urllib3 sets ``Content-Length`` instead of using
    chunked transfer encoding); urllib3 then calls ``read(blocksize)``
    repeatedly during the POST and the progress bar tracks bytes leaving the
    host.
    """

    def __init__(self, file: BinaryIO, file_size: int, filename: str) -> None:
        self.boundary = f"esphomeOTA{secrets.token_hex(16)}"
        prefix = (
            f"--{self.boundary}\r\n"
            f'Content-Disposition: form-data; name="{FORM_FIELD}"; '
            f'filename="{filename}"\r\n'
            f"Content-Type: application/octet-stream\r\n\r\n"
        ).encode()
        suffix = f"\r\n--{self.boundary}--\r\n".encode()
        # Walked in order; ``read()`` advances to the next source on EOF.
        self._sources: list[BinaryIO] = [io.BytesIO(prefix), file, io.BytesIO(suffix)]
        self._idx = 0
        self._total = len(prefix) + file_size + len(suffix)
        self._sent = 0
        self.progress = ProgressBar()

    def __len__(self) -> int:
        return self._total

    @property
    def content_type(self) -> str:
        return f"multipart/form-data; boundary={self.boundary}"

    def read(self, size: int = -1) -> bytes:
        remaining = self._total if size is None or size < 0 else size
        out = bytearray()
        while remaining > 0 and self._idx < len(self._sources):
            chunk = self._sources[self._idx].read(remaining)
            if not chunk:
                self._idx += 1
                continue
            out += chunk
            remaining -= len(chunk)
        if out:
            self._sent += len(out)
            self.progress.update(self._sent / self._total)
        return bytes(out)


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

    if not addr_infos:
        _LOGGER.error("Could not resolve %s", host)
        return 1, None

    file_size = filename.stat().st_size
    _LOGGER.info("Uploading %s (%s bytes) via web_server OTA", filename, file_size)
    auth = HTTPBasicAuth(username, password) if username and password else None

    # Iterate resolved IPs (IPv4 + IPv6 candidates) just like espota2 does.
    for af, _socktype, _, _, sa in addr_infos:
        ip = sa[0]
        # IPv6 literals must be wrapped in brackets in URLs; link-local
        # addresses need a percent-encoded zone index per RFC 6874.
        if af == socket.AF_INET6:
            scope = sa[3] if len(sa) >= 4 else 0
            host_part = f"[{ip}%25{scope}]" if scope else f"[{ip}]"
        else:
            host_part = ip
        url = f"http://{host_part}:{port}{OTA_PATH}"
        _LOGGER.info("Connecting to %s port %s...", ip, port)

        try:
            with open(filename, "rb") as fh:
                streamer = _MultipartStreamer(fh, file_size, filename.name)
                try:
                    response = requests.post(
                        url,
                        data=streamer,
                        auth=auth,
                        timeout=TIMEOUT,
                        headers={
                            "Content-Type": streamer.content_type,
                            "Connection": "close",
                        },
                    )
                finally:
                    streamer.progress.done()
        except requests.RequestException as err:
            _LOGGER.error("OTA upload to %s port %s failed: %s", ip, port, err)
            continue

        if response.status_code == 401:
            raise WebServerOTAError(
                "Authentication failed (HTTP 401). Check the 'web_server' "
                "'auth' username and password."
            )
        if response.status_code != 200:
            detail = response.text.strip() or response.reason or "no response body"
            raise WebServerOTAError(
                f"Unexpected HTTP {response.status_code} response from device: {detail}"
            )

        # The endpoint returns HTTP 200 for both success and failure; the
        # body is what tells us which (see ota_web_server.cpp handleRequest).
        body = response.text.strip()
        if "Successful" in body:
            _LOGGER.info("Device response: %s", body)
            _LOGGER.info("OTA successful")
            return 0, ip

        raise WebServerOTAError(
            f"Device reported OTA failure: {body or 'no response body'}"
        )

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
    for host in hosts:
        try:
            exit_code, used_host = _try_upload(
                host, remote_port, username, password, filename
            )
        except WebServerOTAError as err:
            _LOGGER.error("%s", err)
            continue
        if exit_code == 0:
            return 0, used_host
    # Reached only when every attempt failed; per-attempt errors were
    # already logged. This summary line gives the user an unambiguous
    # "stop reading, nothing worked" marker.
    _LOGGER.error("OTA upload failed.")
    return 1, None
