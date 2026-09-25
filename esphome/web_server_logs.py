"""Stream device logs over the ``web_server`` component's HTTP SSE endpoint.

The ``web_server`` component exposes a Server-Sent Events stream at ``/events``
that multiplexes entity state, keepalive pings, and log lines (``event: log``).
This is the logging counterpart to the web_server OTA upload path
(:mod:`esphome.web_server_ota`); it lets ``esphome logs`` reach a device that
has ``web_server:`` configured but no ``api:``.

Only the ``event: log`` frames are rendered; the payload is the device's
already-formatted, ANSI-colored log line, so it is passed through the same
``LogParser`` + ``safe_print`` path the serial and native-API log viewers use.
The stream is long-lived and the server drops idle connections, so the reader
reconnects automatically until interrupted.
"""

from __future__ import annotations

from datetime import datetime
import logging
import time
from typing import TYPE_CHECKING

import requests
from requests.auth import HTTPBasicAuth

from esphome.core import EsphomeError
from esphome.util import safe_print
from esphome.web_server_helpers import resolve_web_server_urls

if TYPE_CHECKING:
    from aioesphomeapi import LogParser

_LOGGER = logging.getLogger(__name__)

EVENTS_PATH = "/events"
# (connect_timeout, read_timeout). The device sends a keepalive ``ping`` every
# 10s, so a 30s read timeout tolerates a few missed pings before we treat the
# connection as dead and reconnect.
TIMEOUT = (10.0, 30.0)
# Pause between reconnect attempts so a downed device doesn't spin the CPU.
RECONNECT_DELAY = 1.0
# Upper bound for the exponential backoff applied to consecutive failures, so an
# unreachable host backs off instead of retrying (and logging) once a second.
MAX_RECONNECT_DELAY = 10.0


class WebServerLogsError(EsphomeError):
    """Raised when the web_server log stream cannot be used (e.g. bad auth)."""


def _build_urls(hosts: list[str], port: int) -> list[tuple[str, str]]:
    """Resolve ``hosts`` to ``(ip, url)`` pairs for the ``/events`` endpoint."""
    urls: list[tuple[str, str]] = []
    seen: set[str] = set()
    for host in hosts:
        try:
            resolved = resolve_web_server_urls(host, port, EVENTS_PATH)
        except EsphomeError as err:
            _LOGGER.warning("Error resolving IP address of %s: %s", host, err)
            continue
        for ip, url in resolved:
            if url not in seen:
                seen.add(url)
                urls.append((ip, url))
    return urls


def _emit(data_lines: list[str], parser: LogParser) -> None:
    """Render the accumulated ``data:`` lines of one ``event: log`` frame."""
    time_ = datetime.now().astimezone()
    milliseconds = time_.microsecond // 1000
    time_str = (
        f"[{time_.hour:02}:{time_.minute:02}:{time_.second:02}.{milliseconds:03}]"
    )
    for line in data_lines:
        safe_print(parser.parse_line(line, time_str))


def _consume(response: requests.Response, parser: LogParser) -> None:
    """Parse the SSE stream, rendering only ``event: log`` frames.

    Implements the minimal slice of the SSE grammar the ``web_server`` stream
    uses: ``field: value`` lines (with one optional leading space after the
    colon) accumulated until a blank line dispatches the frame. ``id:``,
    ``retry:``, and comment (``:``) lines are ignored, as are non-``log``
    events (``ping``, ``state``, ...).
    """
    event_type = "message"
    data_lines: list[str] = []
    # Iterate bytes and decode as UTF-8 ourselves (matching run_miniterm); the
    # text/event-stream response has no charset, so requests' decode_unicode
    # would fall back to Latin-1 and mojibake UTF-8 log characters.
    for raw in response.iter_lines():
        line = raw.decode("utf8", "backslashreplace")
        if not line:
            if event_type == "log" and data_lines:
                _emit(data_lines, parser)
            event_type = "message"
            data_lines = []
            continue
        if line.startswith(":"):
            continue
        field, _, value = line.partition(":")
        value = value.removeprefix(" ")
        if field == "event":
            event_type = value
        elif field == "data":
            data_lines.append(value)


def _stream(url: str, ip: str, auth: HTTPBasicAuth | None, parser: LogParser) -> bool:
    """Connect and stream one session.

    Returns ``True`` if a connection was established (even if it later
    dropped), ``False`` if the connection attempt itself failed so the caller
    can try the next resolved address.
    """
    connected = False
    _LOGGER.info("Connecting to %s ...", url)
    try:
        with requests.get(
            url,
            stream=True,
            auth=auth,
            timeout=TIMEOUT,
            headers={"Accept": "text/event-stream"},
        ) as response:
            if response.status_code == 401:
                raise WebServerLogsError(
                    "Authentication failed (HTTP 401). Check the 'web_server' "
                    "'auth' username and password."
                )
            if response.status_code in (403, 404):
                # Permanent: the endpoint won't appear on retry (wrong version,
                # 'log' disabled, or forbidden). Surface it instead of looping.
                raise WebServerLogsError(
                    f"Device returned HTTP {response.status_code} for "
                    f"{EVENTS_PATH}; the web_server log stream is unavailable. "
                    "Ensure 'web_server' is version 2 or higher with 'log' enabled."
                )
            if response.status_code != 200:
                _LOGGER.error(
                    "Unexpected HTTP %s response from %s", response.status_code, ip
                )
                return False
            connected = True
            _LOGGER.info("Connected to %s", ip)
            _consume(response, parser)
    except requests.RequestException as err:
        if connected:
            _LOGGER.info("Log stream from %s ended (%s); reconnecting...", ip, err)
        else:
            _LOGGER.warning("Could not connect to %s: %s", ip, err)
    return connected


def run_logs(
    hosts: list[str],
    port: int,
    username: str | None,
    password: str | None,
) -> int:
    """Stream logs from the first reachable host over the web_server SSE feed.

    Reconnects automatically when the stream drops and returns ``0`` on
    ``KeyboardInterrupt`` (Ctrl+C), mirroring how the serial log viewer exits.
    """
    from aioesphomeapi import LogParser

    auth = HTTPBasicAuth(username, password) if username and password else None
    parser = LogParser()
    delay = RECONNECT_DELAY
    try:
        while True:
            if not (urls := _build_urls(hosts, port)):
                _LOGGER.error("Could not resolve any of: %s", ", ".join(hosts))
                connected = False
            else:
                # ``any`` stops at the first address that connects; when that
                # stream drops we reconnect to the same set on the next pass.
                connected = any(_stream(url, ip, auth, parser) for ip, url in urls)
            # Reset the backoff once we reach the device; otherwise grow it
            # (capped) so an unreachable host doesn't retry/log once a second.
            delay = (
                RECONNECT_DELAY if connected else min(delay * 2, MAX_RECONNECT_DELAY)
            )
            time.sleep(delay)
    except KeyboardInterrupt:
        return 0
