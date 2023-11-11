import asyncio
import logging
from datetime import datetime
from typing import Optional

from aioesphomeapi import APIClient, ReconnectLogic, APIConnectionError, LogLevel
from aioesphomeapi.api_pb2 import SubscribeLogsResponse
import zeroconf

from esphome.const import CONF_KEY, CONF_PORT, CONF_PASSWORD, __version__
from esphome.core import CORE
from . import CONF_ENCRYPTION

_LOGGER = logging.getLogger(__name__)


async def async_run_logs(config, address):
    conf = config["api"]
    port: int = int(conf[CONF_PORT])
    password: str = conf[CONF_PASSWORD]
    noise_psk: Optional[str] = None
    if CONF_ENCRYPTION in conf:
        noise_psk = conf[CONF_ENCRYPTION][CONF_KEY]
    _LOGGER.info("Starting log output from %s using esphome API", address)
    cli = APIClient(
        address,
        port,
        password,
        client_info=f"ESPHome Logs {__version__}",
        noise_psk=noise_psk,
    )
    first_connect = True
    dashboard = CORE.dashboard

    def on_log(msg: SubscribeLogsResponse) -> None:
        """Handle a new log message."""
        time_ = datetime.now()
        message: bytes = msg.message
        text = message.decode("utf8", "backslashreplace")
        if dashboard:
            text = text.replace("\033", "\\033")
        print(f"[{time_.hour:02}:{time_.minute:02}:{time_.second:02}]{text}")

    async def on_connect():
        nonlocal first_connect
        try:
            await cli.subscribe_logs(
                on_log,
                log_level=LogLevel.LOG_LEVEL_VERY_VERBOSE,
                dump_config=first_connect,
            )
            first_connect = False
        except APIConnectionError:
            cli.disconnect()

    async def on_disconnect(expected_disconnect: bool) -> None:
        _LOGGER.warning("Disconnected from API")

    zc = zeroconf.Zeroconf()
    reconnect = ReconnectLogic(
        client=cli,
        on_connect=on_connect,
        on_disconnect=on_disconnect,
        zeroconf_instance=zc,
    )
    await reconnect.start()

    try:
        while True:
            await asyncio.sleep(60)
    except KeyboardInterrupt:
        await reconnect.stop()
        zc.close()


def run_logs(config, address):
    asyncio.run(async_run_logs(config, address))
