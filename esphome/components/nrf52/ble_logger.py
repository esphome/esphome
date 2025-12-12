import asyncio
import logging
import re
from typing import Final

from bleak import BleakClient, BleakScanner, BLEDevice
from bleak.exc import (
    BleakCharacteristicNotFoundError,
    BleakDBusError,
    BleakDeviceNotFoundError,
)

_LOGGER = logging.getLogger(__name__)


NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

MAC_ADDRESS_PATTERN: Final = re.compile(
    r"([0-9A-F]{2}[:]){5}[0-9A-F]{2}$", flags=re.IGNORECASE
)


def is_mac_address(value: str) -> bool:
    return MAC_ADDRESS_PATTERN.match(value)


async def logger_scan(name: str) -> BLEDevice | None:
    _LOGGER.info("Scanning bluetooth for %s...", name)
    device = await BleakScanner.find_device_by_name(name)
    if not device:
        _LOGGER.error("%s Bluetooth LE device was not found!", name)
    return device


async def logger_connect(host: str) -> int | None:
    disconnected_event = asyncio.Event()

    def handle_disconnect(client):
        disconnected_event.set()

    def handle_rx(_, data: bytearray):
        print(data.decode("utf-8"), end="")

    _LOGGER.info("Connecting %s...", host)
    try:
        async with BleakClient(host, disconnected_callback=handle_disconnect) as client:
            _LOGGER.info("Connected %s...", host)
            try:
                await client.start_notify(NUS_TX_CHAR_UUID, handle_rx)
            except BleakDBusError as e:
                _LOGGER.error("Bluetooth LE logger: %s", e)
                disconnected_event.set()
            await disconnected_event.wait()
    except BleakDeviceNotFoundError:
        _LOGGER.error("Device %s not found", host)
        return 1
    except BleakCharacteristicNotFoundError:
        _LOGGER.error("Device %s has no NUS characteristic", host)
        return 1
