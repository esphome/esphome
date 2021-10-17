import asyncio
import logging
from datetime import datetime
from typing import Optional

from aioesphomeapi import APIClient, ReconnectLogic, APIConnectionError, LogLevel
import zeroconf

from esphome.const import CONF_KEY, CONF_PORT, CONF_PASSWORD, __version__
from esphome.util import safe_print
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
    zc = zeroconf.Zeroconf()
    cli = APIClient(
        address,
        port,
        password,
        client_info=f"ESPHome Logs {__version__}",
        noise_psk=noise_psk,
    )
    first_connect = True

    def on_log(msg):
        time_ = datetime.now().time().strftime("[%H:%M:%S]")
        text = msg.message.decode("utf8", "backslashreplace")
        safe_print(time_ + text)

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

    async def on_disconnect():
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
