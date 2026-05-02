"""HTTP-based OTA upload via the ``web_server`` component's ``/update`` endpoint.

This is the alternative to ``espota2`` (the native API OTA path). It is useful
when a device only has ``platform: web_server`` configured under ``ota:``, or
when the user has lost the native OTA password but still has ``web_server``
basic-auth credentials.
"""

from __future__ import annotations

import logging
from pathlib import Path
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


class WebServerOTAError(EsphomeError):
    pass


class _ProgressFile:
    """Wraps a file handle so requests' multipart encoder reports progress.

    requests' multipart encoder reads the wrapped file via ``.read(size)``
    while building the body. We forward each read to the underlying file and
    update a :class:`ProgressBar` proportional to bytes consumed. The bar is
    finalized by the caller (see :func:`_try_upload`) rather than from inside
    ``read`` so it always renders cleanly even if urllib3 stops calling
    ``read`` exactly at ``Content-Length`` instead of issuing a trailing
    empty read.
    """

    def __init__(self, file_handle: BinaryIO, file_size: int) -> None:
        self._file = file_handle
        self._size = file_size
        self._read = 0
        self.progress = ProgressBar()

    def read(self, size: int = -1) -> bytes:
        chunk = self._file.read(size)
        if chunk and self._size > 0:
            self._read += len(chunk)
            self.progress.update(self._read / self._size)
        return chunk


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

    auth = HTTPBasicAuth(username, password) if username and password else None

    last_error: str | None = None
    # Each entry is the resolved IP for ``host``; iterate in order so IPv4
    # and IPv6 candidates are both tried (mirrors espota2's behavior).
    for af, _socktype, _, _, sa in addr_infos:
        ip = sa[0]
        # IPv6 literals must be wrapped in brackets in URLs so the port is
        # parsed correctly. ``resolve_ip_address`` can hand back IPv6
        # candidates first on dual-stack networks.
        host_part = f"[{ip}]" if af == socket.AF_INET6 else ip
        url = f"http://{host_part}:{port}{OTA_PATH}"
        _LOGGER.info("Connecting to %s port %s...", ip, port)

        try:
            with open(filename, "rb") as file_handle:
                progress_file = _ProgressFile(file_handle, file_size)
                files = {
                    FORM_FIELD: (
                        filename.name,
                        progress_file,
                        "application/octet-stream",
                    ),
                }
                try:
                    response = requests.post(
                        url,
                        files=files,
                        auth=auth,
                        timeout=TIMEOUT,
                        headers={"Connection": "close"},
                    )
                finally:
                    # Always finalize the progress bar; urllib3 may not issue
                    # a trailing empty read after consuming Content-Length.
                    progress_file.progress.done()
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
            raise WebServerOTAError(
                f"Unexpected HTTP {response.status_code} response from device: "
                f"{response.text.strip()}"
            )

        # The endpoint returns HTTP 200 in both success and failure paths;
        # the body is what tells us which one. See
        # components/web_server/ota/ota_web_server.cpp -> handleRequest().
        body = response.text.strip()
        if "Successful" in body:
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
