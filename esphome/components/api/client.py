from __future__ import annotations

import asyncio
from datetime import datetime
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

from esphome.const import CONF_KEY, CONF_PASSWORD, CONF_PORT, __version__
from esphome.core import CORE

from . import CONF_ENCRYPTION

if TYPE_CHECKING:
    from aioesphomeapi.api_pb2 import (
        SubscribeLogsResponse,  # pylint: disable=no-name-in-module
    )


_LOGGER = logging.getLogger(__name__)


async def async_run_logs(config: dict[str, Any], addresses: list[str]) -> None:
    """Run the logs command in the event loop."""
    conf = config["api"]
    name = config["esphome"]["name"]
    port: int = int(conf[CONF_PORT])
    password: str = conf[CONF_PASSWORD]
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
        password,
        client_info=f"ESPHome Logs {__version__}",
        noise_psk=noise_psk,
        addresses=addresses,  # Pass all addresses for automatic retry
    )
    dashboard = CORE.dashboard

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
            print(parsed_msg.replace("\033", "\\033") if dashboard else parsed_msg)

    stop = await async_run(cli, on_log, name=name)
    try:
        await asyncio.Event().wait()
    finally:
        await stop()


def run_logs(config: dict[str, Any], addresses: list[str]) -> None:
    """Run the logs command."""
    with contextlib.suppress(KeyboardInterrupt):
        asyncio.run(async_run_logs(config, addresses))
