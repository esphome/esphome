from __future__ import annotations

import asyncio
from collections import deque
from contextlib import suppress
from datetime import datetime
import logging
import re
from typing import TYPE_CHECKING, Any
import warnings

# Suppress protobuf version warnings
with warnings.catch_warnings():
    warnings.filterwarnings(
        "ignore", category=UserWarning, message=".*Protobuf gencode version.*"
    )
    from aioesphomeapi import APIClient, parse_log_message
    from aioesphomeapi.log_runner import async_run

from esphome import platform_hooks
from esphome.const import CONF_ENCRYPTION, CONF_KEY, CONF_PORT, __version__
from esphome.core import CORE
from esphome.util import safe_print

if TYPE_CHECKING:
    from aioesphomeapi.api_pb2 import (
        SubscribeLogsResponse,  # pylint: disable=no-name-in-module
    )


_LOGGER = logging.getLogger(__name__)


# Necessary condition for a line to decode to anything: every frame or
# register the platform decoders match carries either a 0x-prefixed
# pointer (nrf52's variable-length ``PC=0x27a1c``, esp32's
# ``Backtrace: 0x400d1a2c:...``) or a bare 8-hex-digit word (esp8266
# stack dumps). A unit test feeds one real dump line per registered
# platform through this to keep the superset claim honest. Deliberately
# not the crash-marker grammar - that lives in the platform decoders
# and a copy here would drift. The device builder gates its decoder
# subprocess the same way (esphome_device_builder/controllers/devices/
# backtrace.py).
_ADDRESS_RE = re.compile(r"0x[0-9a-fA-F]{4,}\b|\b[0-9a-fA-F]{8}\b")

# Lines kept for replay once decoding starts. Crash markers without an
# address (esp8266's ``>>>stack>>>``, panic banners) precede the first
# address-bearing line by at most a few lines; replaying them lets the
# decoder enter its dump region as if it had seen the stream live.
_REPLAY_LINES = 8


class LogLineProcessor:
    """Feeds incoming log lines to the stack-trace decoder.

    Three responsibilities beyond just calling the decoder:
    1. Resolve the platform decoder lazily. Importing a platform package
       pulls in the validation stack, so nothing is imported until a
       line carries something that could decode (see _ADDRESS_RE); the
       preceding context lines replay from a small buffer so multi-line
       dumps whose opening marker has no address still decode. Sessions
       that never see a crash-shaped line never pay the import.
    2. Catch everything the decoder can raise. aioesphomeapi isolates
       exceptions raised by log handlers, so an escaping one no longer
       kills the session, but it does log a full traceback per line. A
       crash dump carries a PC line plus one per backtrace frame, so the
       tracebacks bury the dump the user is trying to read. Decoding is a
       diagnostic nicety; nothing it raises is worth that noise.
    3. Disable decoding after the first failure. _decode_pc shells out to
       the toolchain to resolve addr2line, which is expensive; a single
       crash dump can contain many PC/BT lines and we don't want to retry
       the failing subprocess for each one. This only works if every
       failure is caught, which is why 2 is not narrowed to EsphomeError.
    """

    def __init__(self, config: dict[str, Any], platform: str) -> None:
        self._config = config
        self._platform = platform
        self._platform_handler: Any | None = None
        self._decode_enabled = True
        self._recent: deque[str] = deque(maxlen=_REPLAY_LINES)
        self.backtrace_state = False
        # The registry can prove some platforms have no analyzer without
        # importing anything; resolve those now so the unavailable notice
        # fires at session start (as it always did) and the per-line gate
        # never runs.
        if not platform_hooks.may_provide_hook(platform, "process_stacktrace"):
            self._resolve_handler()

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled:
            return
        if self._platform_handler is None:
            if not _ADDRESS_RE.search(raw_line):
                self._recent.append(raw_line)
                return
            if not self._resolve_handler():
                return
            pending = list(self._recent)
            self._recent.clear()
            for buffered in pending:
                self._feed(buffered)
                if not self._decode_enabled:
                    return
        self._feed(raw_line)

    def _resolve_handler(self) -> bool:
        handler = platform_hooks.get_stacktrace_handler(self._platform)
        if handler is None:
            self._decode_enabled = False
            return False
        self._platform_handler = handler
        return True

    def _feed(self, raw_line: str) -> None:
        try:
            self.backtrace_state = self._platform_handler(
                self._config, raw_line, self.backtrace_state
            )
        except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-except
            self._decode_enabled = False
            self.backtrace_state = False
            # _run_idedata raises EsphomeError with no message; fall back
            # to a generic explanation when str(exc) is empty.
            detail = str(exc) or "build artifacts not found locally"
            _LOGGER.debug("Stack-trace decoding failed", exc_info=True)
            _LOGGER.warning(
                "Crash trace decoding unavailable: %s. "
                "Run 'esphome compile' for this device to enable PC decoding.",
                detail,
            )


async def async_run_logs(
    config: dict[str, Any],
    addresses: list[str],
    subscribe_states: bool = True,
) -> None:
    """Run the logs command in the event loop."""
    conf = config["api"]
    name = config["esphome"]["name"]
    port: int = int(conf[CONF_PORT])
    noise_psk: str | None = None
    if (encryption := conf.get(CONF_ENCRYPTION)) and (key := encryption.get(CONF_KEY)):
        noise_psk = key

    _LOGGER.info(
        "Starting log output from %s using esphome API", " or ".join(addresses)
    )

    cli = APIClient(
        addresses[0],  # Primary address for compatibility
        port,
        "",  # Password auth removed in 2026.1.0
        client_info=f"ESPHome Logs {__version__}",
        noise_psk=noise_psk,
        addresses=addresses,  # Pass all addresses for automatic retry
        provide_time=False,
    )

    # The platform stacktrace decoder resolves lazily inside the
    # processor: no platform package is imported unless a crash-shaped
    # line actually arrives.
    processor = LogLineProcessor(config, CORE.target_platform)

    def on_log(msg: SubscribeLogsResponse) -> None:
        """Handle a new log message."""
        time_ = datetime.now().astimezone()
        message: bytes = msg.message
        text = message.decode("utf8", "backslashreplace")
        nanoseconds = time_.microsecond // 1000
        timestamp = (
            f"[{time_.hour:02}:{time_.minute:02}:{time_.second:02}.{nanoseconds:03}]"
        )
        for parsed_msg in parse_log_message(text, timestamp):
            # safe_print handles the dashboard \033 escaping and falls back
            # to backslashreplace encoding on stdouts that can't represent
            # the wifi signal-bar block characters (Windows redirected
            # cp1252 pipe).
            safe_print(parsed_msg)
        for raw_line in text.splitlines():
            processor.process_line(raw_line)

    # Safe to fall back to plaintext here only for this diagnostics use
    # case: the stream is one-way from device to client, and this code
    # never accepts commands or acts on any message the device sends.
    # An on-path attacker could still both inject fabricated log lines
    # and passively read the device's log output (and any state data
    # delivered when subscribe_states is enabled), so this does lose
    # confidentiality as well as authentication/integrity. That tradeoff
    # is acceptable for operator-visible logs, which aioesphomeapi also
    # warns may come from an unverified device. Never mirror this opt-in
    # for any connection that sends data to the device or uses Home
    # Assistant actions.
    stop = await async_run(
        cli,
        on_log,
        name=name,
        subscribe_states=subscribe_states,
        allow_plaintext_fallback=True,
        # A top-level ``deep_sleep:`` block means the device is only awake
        # briefly; cap the reconnect backoff so a wake window is not missed.
        deep_sleep="deep_sleep" in config,
    )
    try:
        await asyncio.Event().wait()
    finally:
        await stop()


def run_logs(
    config: dict[str, Any],
    addresses: list[str],
    subscribe_states: bool = True,
) -> None:
    """Run the logs command."""
    with suppress(KeyboardInterrupt):
        asyncio.run(
            async_run_logs(config, addresses, subscribe_states=subscribe_states)
        )
