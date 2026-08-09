from __future__ import annotations

import asyncio
from contextlib import suppress
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

from esphome.const import CONF_ENCRYPTION, CONF_KEY, CONF_PORT, __version__
from esphome.core import CORE
from esphome.stacktrace import LogLineProcessor
from esphome.util import safe_print

if TYPE_CHECKING:
    from aioesphomeapi.api_pb2 import (
        SubscribeLogsResponse,  # pylint: disable=no-name-in-module
    )


_LOGGER = logging.getLogger(__name__)


async def async_run_logs(
    config: dict[str, Any],
    addresses: list[str],
    subscribe_states: bool = True,
) -> None:
    """Run the logs command in the event loop."""
    from datetime import datetime

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

    # Decoder resolution policy lives in LogLineProcessor.
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
