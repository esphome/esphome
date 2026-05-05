from __future__ import annotations

import asyncio
from datetime import datetime
import importlib
import logging
from typing import TYPE_CHECKING, Any
import warnings

# Suppress protobuf version warnings
with warnings.catch_warnings():
    warnings.filterwarnings(
        "ignore", category=UserWarning, message=".*Protobuf gencode version.*"
    )
    from aioesphomeapi import APIClient, parse_log_message
    from aioesphomeapi.log_runner import async_run

import contextlib

from esphome.const import CONF_KEY, CONF_PORT, __version__
from esphome.core import CORE, EsphomeError
from esphome.util import safe_print

from . import CONF_ENCRYPTION

if TYPE_CHECKING:
    from aioesphomeapi.api_pb2 import (
        SubscribeLogsResponse,  # pylint: disable=no-name-in-module
    )


_LOGGER = logging.getLogger(__name__)


class _LogLineProcessor:
    """Feeds incoming log lines to the stack-trace decoder.

    Two responsibilities beyond just calling the decoder:
    1. Catch EsphomeError. on_log runs inside an asyncio protocol
       callback; if an exception escapes, the loop tears the transport
       down with "Fatal error: protocol.data_received() call failed."
       and ReconnectLogic immediately reconnects, the device replays
       the same crash trace, and we loop forever.
    2. Disable decoding after the first failure. _decode_pc shells out
       to PlatformIO via _run_idedata, which is expensive; a single
       crash dump can contain many PC/BT lines and we don't want to
       retry the failing subprocess for each one.
    """

    def __init__(self, config: dict[str, Any], platform_handler: Any | None) -> None:
        self._config = config
        self._platform_handler = platform_handler
        self._decode_enabled = True
        self.backtrace_state = False

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled:
            return
        try:
            if self._platform_handler is not None:
                self.backtrace_state = self._platform_handler(
                    self._config, raw_line, self.backtrace_state
                )
        except EsphomeError as exc:
            self._decode_enabled = False
            self.backtrace_state = False
            # _run_idedata raises EsphomeError with no message; fall back
            # to a generic explanation when str(exc) is empty.
            detail = str(exc) or "build artifacts not found locally"
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

    if len(addresses) == 1:
        _LOGGER.info("Starting log output from %s using esphome API", addresses[0])
    else:
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
    )

    # Try platform-specific stacktrace handler first, fall back to generic
    platform_process_stacktrace = None
    try:
        module = importlib.import_module("esphome.components." + CORE.target_platform)
        platform_process_stacktrace = getattr(module, "process_stacktrace")
    except (AttributeError, ImportError):
        _LOGGER.info(
            'Stacktrace analysis is unavailable: no compatible analyzer found for target platform "%s".',
            CORE.target_platform,
        )

    processor = _LogLineProcessor(config, platform_process_stacktrace)

    def on_log(msg: SubscribeLogsResponse) -> None:
        """Handle a new log message."""
        time_ = datetime.now()
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
    with contextlib.suppress(KeyboardInterrupt):
        asyncio.run(
            async_run_logs(config, addresses, subscribe_states=subscribe_states)
        )
