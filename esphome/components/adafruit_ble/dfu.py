"""Host-side Adafruit nRF52 bootloader BLE OTA (Nordic legacy DFU).

The Adafruit bootloader only serves its DFU service while it is in DFU mode, so
an update has two steps: ask the running application to reboot into the
bootloader (the trigger service built into the ``adafruit_ble`` OTA component),
then push the DFU package to the bootloader itself.

Reference implementation of the protocol:
https://github.com/recrof/nrf_dfu_py/blob/main/dfu_lib.py
"""

from __future__ import annotations

import asyncio
from contextlib import suppress
import json
import logging
from pathlib import Path
import re
import struct
import time
import zipfile

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakDeviceNotFoundError, BleakError

from esphome.core import EsphomeError
from esphome.espota2 import ProgressBar

_LOGGER = logging.getLogger(__name__)

# Nordic legacy DFU service, used by the Adafruit nRF52 bootloader.
DFU_SERVICE_UUID = "00001530-1212-efde-1523-785feabcd123"
DFU_CONTROL_POINT_UUID = "00001531-1212-efde-1523-785feabcd123"
DFU_PACKET_UUID = "00001532-1212-efde-1523-785feabcd123"

# ESPHome DFU trigger service, exposed by the application (ota_adafruit_ble.cpp).
# Kept distinct from the bootloader's DFU service so the two can be told apart.
TRIGGER_SERVICE_UUID = "e5b10001-9c3a-4f5d-8a1b-2c3d4e5f6071"
TRIGGER_CHARACTERISTIC_UUID = "e5b10002-9c3a-4f5d-8a1b-2c3d4e5f6071"
TRIGGER_MAGIC = b"DFU"

_OP_START_DFU = 0x01
_OP_INIT_DFU_PARAMS = 0x02
_OP_RECEIVE_FIRMWARE_IMAGE = 0x03
_OP_VALIDATE = 0x04
_OP_ACTIVATE_AND_RESET = 0x05
_OP_RESET = 0x06
_OP_PACKET_RECEIPT_NOTIF_REQ = 0x08
_OP_RESPONSE_CODE = 0x10
_OP_PACKET_RECEIPT_NOTIF = 0x11

_UPLOAD_MODE_APPLICATION = 0x04
_RESPONSE_SUCCESS = 0x01

# The bootloader runs out of memory when more than 8 packets are unacknowledged.
_PRN = 8
# The bootloader needs a moment between START_DFU and the image size packet;
# sending the size too early leaves the start command unanswered.
_START_DELAY = 0.6
# A dropped link mid-update is usually transient, so the sequence is retried.
_ATTEMPTS = 3
_RETRY_DELAY = 3.0
# ATT payload cap; the negotiated MTU is normally smaller.
_MAX_CHUNK_SIZE = 244
_SCAN_TIMEOUT = 10.0
_RESPONSE_TIMEOUT = 30.0
# Time the bootloader needs to start advertising after the DFU request.
_REBOOT_TIMEOUT = 20.0

_MAC_ADDRESS_PATTERN = re.compile(r"([0-9A-F]{2}:){5}[0-9A-F]{2}$", flags=re.IGNORECASE)


class _DfuSession:
    """Control point notifications and packet receipt events of one DFU run."""

    def __init__(self) -> None:
        self._responses: asyncio.Queue[tuple[int, int]] = asyncio.Queue()
        self.packet_receipt = asyncio.Event()

    def notification_handler(self, _sender: int, data: bytearray) -> None:
        opcode = data[0]
        if opcode == _OP_RESPONSE_CODE and len(data) >= 3:
            self._responses.put_nowait((data[1], data[2]))
        elif opcode == _OP_PACKET_RECEIPT_NOTIF:
            self.packet_receipt.set()

    async def wait_for_response(
        self, request_op: int, timeout: float = _RESPONSE_TIMEOUT
    ) -> int:
        try:
            response_op, status = await asyncio.wait_for(self._responses.get(), timeout)
        except TimeoutError as err:
            raise EsphomeError(
                f"DFU timed out waiting for the response to opcode {request_op:#04x}"
            ) from err
        if response_op != request_op:
            raise EsphomeError(
                f"DFU got a response for opcode {response_op:#04x}, "
                f"expected {request_op:#04x}"
            )
        return status


def _service_uuids(advertisement) -> set[str]:
    return {uuid.lower() for uuid in (advertisement.service_uuids or [])}


def _next_address(address: str) -> str | None:
    """The Adafruit bootloader can come up one address above the application."""
    if not _MAC_ADDRESS_PATTERN.match(address):
        return None
    prefix, last = address[:-2], int(address[-2:], 16)
    return f"{prefix}{((last + 1) & 0xFF):02X}"


def _load_firmware(firmware: Path) -> tuple[bytes, bytes, int]:
    """Return (application image, init packet, image size) of a DFU package."""
    if not firmware.is_file():
        raise EsphomeError(
            f"DFU package not found: {firmware}. Compile the firmware first."
        )
    with zipfile.ZipFile(firmware) as archive:
        names = archive.namelist()
        bin_file = dat_file = None
        if "manifest.json" in names:
            manifest = json.loads(archive.read("manifest.json"))
            application = manifest.get("manifest", manifest).get("application")
            if application is None:
                raise EsphomeError(f"{firmware} is not an application update package")
            bin_file = application.get("bin_file")
            dat_file = application.get("dat_file")
        else:
            bin_file = next((name for name in names if name.endswith(".bin")), None)
            dat_file = next((name for name in names if name.endswith(".dat")), None)
        if bin_file is None or dat_file is None:
            raise EsphomeError(f"Could not find the application image in {firmware}")
        image = archive.read(bin_file)
        init_packet = archive.read(dat_file)
    if not image:
        raise EsphomeError(f"Empty application image in {firmware}")
    return image, init_packet, len(image)


async def list_services(address: str) -> set[str]:
    """Return the GATT service UUIDs of ``address`` (lowercase)."""
    try:
        client = BleakClient(address, timeout=_SCAN_TIMEOUT)
        async with client:
            return {str(service.uuid).lower() for service in client.services}
    except BleakDeviceNotFoundError as err:
        raise EsphomeError(f"Device {address} was not found") from err
    except (BleakError, OSError) as err:
        raise EsphomeError(f"Could not connect to {address}: {err}") from err


async def request_dfu_mode(address: str) -> None:
    """Ask the running application to reboot into the bootloader's DFU mode."""
    _LOGGER.info("Requesting DFU mode from %s...", address)
    client = BleakClient(address, timeout=_SCAN_TIMEOUT)
    try:
        await client.connect()
    except BleakDeviceNotFoundError as err:
        raise EsphomeError(f"Device {address} was not found") from err
    except (BleakError, OSError) as err:
        raise EsphomeError(f"Could not connect to {address}: {err}") from err
    try:
        await client.write_gatt_char(
            TRIGGER_CHARACTERISTIC_UUID, TRIGGER_MAGIC, response=True
        )
    except (BleakError, OSError) as err:
        # The application reboots as soon as the write is handled, so a
        # disconnect while waiting for the response is expected. Windows
        # reports that as a cancelled operation (OSError), not a BleakError.
        _LOGGER.debug("DFU mode request ended with %s", err)
    finally:
        with suppress(Exception):
            await client.disconnect()


async def find_dfu_bootloader(addresses: list[str]) -> str:
    """Scan until a device advertising the DFU service shows up."""
    _LOGGER.info("Scanning for the DFU bootloader...")
    preferred = {address.lower() for address in addresses}
    for address in addresses:
        if (next_address := _next_address(address)) is not None:
            preferred.add(next_address.lower())
    deadline = time.monotonic() + _REBOOT_TIMEOUT
    while True:
        devices = await BleakScanner.discover(timeout=_SCAN_TIMEOUT, return_adv=True)
        candidates = {
            device_address.lower(): device_address
            for device_address, (_, advertisement) in devices.items()
            if DFU_SERVICE_UUID in _service_uuids(advertisement)
        }
        for address in preferred:
            if address in candidates:
                return candidates[address]
        if candidates:
            return next(iter(candidates.values()))
        if time.monotonic() >= deadline:
            raise EsphomeError(
                "The DFU bootloader was not found. Make sure the device is in DFU mode."
            )


async def _write_control(client: BleakClient, op: int, *payload: int) -> None:
    await client.write_gatt_char(
        DFU_CONTROL_POINT_UUID, bytes([op, *payload]), response=True
    )


async def _stream_firmware(
    client: BleakClient, session: _DfuSession, image: bytes
) -> None:
    mtu = getattr(client, "mtu_size", 23)
    chunk_size = max(20, min(mtu - 3, _MAX_CHUNK_SIZE))
    _LOGGER.debug("DFU uses %d byte chunks for %d bytes", chunk_size, len(image))
    progress = ProgressBar("Uploading")
    progress.update(0)
    packets = 0
    try:
        for offset in range(0, len(image), chunk_size):
            chunk = image[offset : offset + chunk_size]
            await client.write_gatt_char(DFU_PACKET_UUID, chunk, response=False)
            packets += 1
            progress.update((offset + len(chunk)) / len(image))
            if packets >= _PRN:
                packets = 0
                session.packet_receipt.clear()
                with suppress(asyncio.TimeoutError):
                    await asyncio.wait_for(session.packet_receipt.wait(), timeout=2.0)
    finally:
        progress.done()


async def _perform_update(
    client: BleakClient, image: bytes, init_packet: bytes
) -> None:
    session = _DfuSession()
    await client.start_notify(DFU_CONTROL_POINT_UUID, session.notification_handler)
    try:
        _LOGGER.debug("DFU: starting the update")
        await _write_control(client, _OP_START_DFU, _UPLOAD_MODE_APPLICATION)
        await asyncio.sleep(_START_DELAY)
        await client.write_gatt_char(
            DFU_PACKET_UUID, struct.pack("<III", 0, 0, len(image)), response=False
        )
        status = await session.wait_for_response(_OP_START_DFU, timeout=60.0)
        if status != _RESPONSE_SUCCESS:
            await _write_control(client, _OP_RESET)
            raise EsphomeError(f"DFU start failed with status {status:#04x}")

        _LOGGER.debug("DFU: sending the init packet")
        await _write_control(client, _OP_INIT_DFU_PARAMS, 0x00)
        await client.write_gatt_char(DFU_PACKET_UUID, init_packet, response=False)
        await _write_control(client, _OP_INIT_DFU_PARAMS, 0x01)
        status = await session.wait_for_response(_OP_INIT_DFU_PARAMS)
        if status != _RESPONSE_SUCCESS:
            raise EsphomeError(f"DFU init packet failed with status {status:#04x}")

        await _write_control(
            client, _OP_PACKET_RECEIPT_NOTIF_REQ, *struct.pack("<H", _PRN)
        )
        _LOGGER.debug("DFU: streaming the image")
        await _write_control(client, _OP_RECEIVE_FIRMWARE_IMAGE)
        await _stream_firmware(client, session, image)
        status = await session.wait_for_response(
            _OP_RECEIVE_FIRMWARE_IMAGE, timeout=max(60.0, len(image) / 50000)
        )
        if status != _RESPONSE_SUCCESS:
            raise EsphomeError(f"DFU upload failed with status {status:#04x}")

        await _write_control(client, _OP_VALIDATE)
        status = await session.wait_for_response(_OP_VALIDATE)
        if status != _RESPONSE_SUCCESS:
            raise EsphomeError(f"DFU validation failed with status {status:#04x}")

        # The bootloader resets as soon as it handles this, so the link drops
        # before the write can be acknowledged (an OSError on Windows).
        with suppress(BleakError, OSError):
            await _write_control(client, _OP_ACTIVATE_AND_RESET)
    finally:
        with suppress(Exception):
            await client.stop_notify(DFU_CONTROL_POINT_UUID)


async def _dfu_attempt(address: str, image: bytes, init_packet: bytes) -> None:
    """Run the whole DFU sequence once."""
    client = BleakClient(address, timeout=_SCAN_TIMEOUT)
    try:
        await client.connect()
    except BleakDeviceNotFoundError as err:
        raise EsphomeError(f"Device {address} was not found") from err
    except (BleakError, OSError) as err:
        raise EsphomeError(f"Could not connect to {address}: {err}") from err
    try:
        await _perform_update(client, image, init_packet)
    except (BleakError, OSError) as err:
        raise EsphomeError(f"DFU failed: {err}") from err
    finally:
        with suppress(Exception):
            await client.disconnect()


async def dfu_upload(address: str, firmware: Path) -> None:
    """Flash ``firmware`` (an Adafruit DFU package) to ``address``."""
    image, init_packet, size = _load_firmware(firmware)
    for attempt in range(1, _ATTEMPTS + 1):
        _LOGGER.info(
            "Uploading %s to %s (%d bytes)%s...",
            firmware.name,
            address,
            size,
            f", attempt {attempt}/{_ATTEMPTS}" if attempt > 1 else "",
        )
        try:
            await _dfu_attempt(address, image, init_packet)
        except EsphomeError as err:
            if attempt == _ATTEMPTS:
                raise
            _LOGGER.warning("DFU attempt %d/%d failed: %s", attempt, _ATTEMPTS, err)
            await asyncio.sleep(_RETRY_DELAY)
        else:
            _LOGGER.info("DFU finished; the device restarts into the new firmware")
            return
