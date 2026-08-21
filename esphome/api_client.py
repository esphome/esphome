from __future__ import annotations

import asyncio
from contextlib import suppress
import logging
import threading
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
    from collections.abc import Callable

    from aioesphomeapi.api_pb2 import (
        SubscribeLogsResponse,  # pylint: disable=no-name-in-module
    )


_LOGGER = logging.getLogger(__name__)


async def async_run_logs(
    config: dict[str, Any],
    addresses: list[str],
    subscribe_states: bool = True,
    mqtt_resolver: Callable[[threading.Event], list[str]] | None = None,
) -> None:
    """Run the logs command in the event loop.

    If ``mqtt_resolver`` is given, it is called in a worker thread (paho-mqtt
    has no asyncio support on Windows) concurrently with the connection
    attempts to ``addresses``, and any addresses it discovers are fed into
    the running client. It owns its own failure handling (returning [] when
    discovery fails) and must honor the ``threading.Event`` it is passed so
    teardown is not delayed by the lookup's wait window; the initial broker
    connect itself is only bounded by the socket timeout.
    """
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

    mqtt_task: asyncio.Task[None] | None = None
    mqtt_stop_event = threading.Event()

    def _cancel_mqtt_discovery() -> None:
        """Stop the broker lookup once a connection has been established.

        Its answer is only useful while still disconnected: after that it
        either duplicates the connected address or arrives too late to
        matter, so don't keep an idle broker session open for it.
        """
        mqtt_stop_event.set()
        if mqtt_task is not None and not mqtt_task.done():
            mqtt_task.cancel()

    async def _resolve_mqtt_addresses() -> None:
        """Discover the device address via the MQTT broker in the background."""
        try:
            mqtt_ips = await asyncio.to_thread(mqtt_resolver, mqtt_stop_event)
            if not mqtt_ips:
                _LOGGER.debug(
                    "MQTT discovery %s",
                    "aborted" if mqtt_stop_event.is_set() else "found no addresses",
                )
                return
            if cli.add_addresses(mqtt_ips):
                _LOGGER.info("Discovered address(es) via MQTT: %s", ", ".join(mqtt_ips))
            else:
                _LOGGER.debug(
                    "MQTT-discovered address(es) already known: %s", ", ".join(mqtt_ips)
                )
        except Exception:  # pylint: disable=broad-except
            # A background task failure would otherwise stay invisible for
            # the whole session and only re-raise at teardown
            _LOGGER.exception("MQTT address discovery failed")

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
        on_connect=_cancel_mqtt_discovery if mqtt_resolver is not None else None,
    )
    try:
        # Don't start (or keep) the broker lookup if a connection already
        # succeeded; the stop event doubles as the not-needed-anymore latch
        # and get_esphome_device_ip returns immediately when it is set.
        if mqtt_resolver is not None and not mqtt_stop_event.is_set():
            mqtt_task = asyncio.create_task(_resolve_mqtt_addresses())
        await asyncio.Event().wait()
    finally:
        try:
            if mqtt_task is not None:
                # Unblock the worker thread first so it can't hold up
                # loop.shutdown_default_executor() for the full lookup timeout.
                mqtt_stop_event.set()
                # Give the worker a moment to exit through its own error
                # handling; cancelling first would race out a late failure.
                done, _ = await asyncio.wait([mqtt_task], timeout=1.0)
                if not done:
                    mqtt_task.cancel()
                # return_exceptions keeps a CancelledError from the cancel()
                # above from re-raising here and jumping over the stop() below.
                # The task handles Exception itself, so only a BaseException
                # escape (e.g. SystemExit from the worker) can land here.
                (result,) = await asyncio.gather(mqtt_task, return_exceptions=True)
                if isinstance(result, BaseException) and not isinstance(
                    result, asyncio.CancelledError
                ):
                    _LOGGER.error("MQTT address discovery failed", exc_info=result)
        finally:
            # Must run even if a second cancellation lands mid-cleanup above
            await stop()


def run_logs(
    config: dict[str, Any],
    addresses: list[str],
    subscribe_states: bool = True,
    mqtt_resolver: Callable[[threading.Event], list[str]] | None = None,
) -> None:
    """Run the logs command."""
    with suppress(KeyboardInterrupt):
        asyncio.run(
            async_run_logs(
                config,
                addresses,
                subscribe_states=subscribe_states,
                mqtt_resolver=mqtt_resolver,
            )
        )
