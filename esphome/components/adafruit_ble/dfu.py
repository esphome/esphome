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
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager, suppress
import json
import logging
from pathlib import Path
import struct
import zipfile

from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData
from bleak.exc import BleakDBusError, BleakDeviceNotFoundError, BleakError

from esphome.components.nrf52.ble_logger import MAC_ADDRESS_PATTERN
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
# Starting the update erases flash and finishing it writes the last pages.
_LONG_RESPONSE_TIMEOUT = 60.0
_RECEIPT_TIMEOUT = 2.0
# Time the bootloader needs to start advertising after the DFU request.
_REBOOT_TIMEOUT = 20.0


class _DfuSession:
    """Control point notifications and packet receipts of one DFU run."""

    def __init__(self) -> None:
        self._responses: asyncio.Queue[tuple[int, int]] = asyncio.Queue()
        self._receipt = asyncio.Event()
        self.receipts = 0

    def notification_handler(self, _sender: int, data: bytearray) -> None:
        opcode = data[0]
        if opcode == _OP_RESPONSE_CODE and len(data) >= 3:
            self._responses.put_nowait((data[1], data[2]))
        elif opcode == _OP_PACKET_RECEIPT_NOTIF:
            self.receipts += 1
            self._receipt.set()

    async def expect_success(
        self, request_op: int, step: str, timeout: float = _RESPONSE_TIMEOUT
    ) -> None:
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
        if status != _RESPONSE_SUCCESS:
            raise EsphomeError(f"DFU {step} failed with status {status:#04x}")

    async def wait_for_receipts(self, count: int) -> None:
        """Wait until ``count`` packet receipts have arrived in total."""
        try:
            async with asyncio.timeout(_RECEIPT_TIMEOUT):
                while self.receipts < count:
                    # Cleared only while the count is short, so no receipt is lost
                    self._receipt.clear()
                    await self._receipt.wait()
        except TimeoutError as err:
            raise EsphomeError(
                f"DFU timed out waiting for packet receipt {count}"
            ) from err


def _service_uuids(advertisement) -> set[str]:
    return {uuid.lower() for uuid in (advertisement.service_uuids or [])}


def _next_address(address: str) -> str | None:
    """The Adafruit bootloader can come up one address above the application."""
    if not MAC_ADDRESS_PATTERN.match(address):
        return None
    prefix, last = address[:-2], int(address[-2:], 16)
    return f"{prefix}{((last + 1) & 0xFF):02X}"


def _load_firmware(firmware: Path) -> tuple[bytes, bytes]:
    """Return (application image, init packet) of a DFU package."""
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
    return image, init_packet


class _DeviceNotFoundError(EsphomeError):
    """The device did not answer at the address it was asked for."""


def _raise_if_not_paired(address: str, err: Exception) -> None:
    if isinstance(err, BleakDBusError) and "NotPermitted" in err.dbus_error:
        raise EsphomeError(
            f"Cannot connect to {address}: Make sure the device is paired."
        ) from err


@asynccontextmanager
async def _connect(address: str) -> AsyncIterator[BleakClient]:
    client = BleakClient(address, timeout=_SCAN_TIMEOUT)
    try:
        await client.connect()
    except BleakDeviceNotFoundError as err:
        raise _DeviceNotFoundError(f"Device {address} was not found") from err
    except (BleakError, OSError) as err:
        _raise_if_not_paired(address, err)
        raise EsphomeError(f"Could not connect to {address}: {err}") from err
    try:
        yield client
    finally:
        with suppress(Exception):
            await client.disconnect()


async def enter_dfu_mode(address: str) -> str:
    """Return the address of the bootloader in DFU mode.

    A running application is first asked to reboot into the bootloader. A
    device that is not found may still be in its bootloader from a failed
    upload, which can advertise one address above the application.
    """
    try:
        async with _connect(address) as client:
            services = {str(service.uuid).lower() for service in client.services}
            if DFU_SERVICE_UUID in services:
                return address
            if TRIGGER_SERVICE_UUID not in services:
                raise EsphomeError(
                    f"{address} does not offer an OTA service. Put the device "
                    "into DFU mode and try again."
                )
            _LOGGER.info("Requesting DFU mode from %s...", address)
            try:
                await client.write_gatt_char(
                    TRIGGER_CHARACTERISTIC_UUID, TRIGGER_MAGIC, response=True
                )
            except (BleakError, OSError) as err:
                _raise_if_not_paired(address, err)
                # The application reboots as soon as the write is handled, so a
                # disconnect while waiting for the response is expected. Windows
                # reports that as a cancelled operation (OSError), not a BleakError.
                _LOGGER.debug("DFU mode request ended with %s", err)
    except _DeviceNotFoundError:
        _LOGGER.info("%s was not found, looking for its DFU bootloader", address)
    return await find_dfu_bootloader(address)


async def find_dfu_bootloader(address: str) -> str:
    """Scan for the bootloader of the device that was asked to enter DFU mode.

    Only ``address`` and the address above it are accepted, so a different
    device that happens to be in DFU mode nearby is never flashed.
    """
    _LOGGER.info("Scanning for the DFU bootloader...")
    wanted = {address.lower()}
    if (next_address := _next_address(address)) is not None:
        wanted.add(next_address.lower())
    others: set[str] = set()

    def _is_bootloader(device: BLEDevice, advertisement: AdvertisementData) -> bool:
        if DFU_SERVICE_UUID not in _service_uuids(advertisement):
            return False
        if device.address.lower() in wanted:
            return True
        others.add(device.address)
        return False

    device = await BleakScanner.find_device_by_filter(
        _is_bootloader, timeout=_REBOOT_TIMEOUT
    )
    if device is not None:
        return device.address
    message = f"The DFU bootloader of {address} was not found."
    if others:
        message += (
            f" Other devices in DFU mode: {', '.join(sorted(others))}. Pass the "
            "right one with --device."
        )
    raise EsphomeError(message)


async def _write_control(client: BleakClient, op: int, *payload: int) -> None:
    await client.write_gatt_char(
        DFU_CONTROL_POINT_UUID, bytes([op, *payload]), response=True
    )


async def _stream_firmware(
    client: BleakClient, session: _DfuSession, image: bytes
) -> None:
    chunk_size = max(20, min(client.mtu_size - 3, _MAX_CHUNK_SIZE))
    _LOGGER.debug("DFU uses %d byte chunks for %d bytes", chunk_size, len(image))
    progress = ProgressBar("Uploading")
    progress.update(0)
    try:
        for packet, offset in enumerate(range(0, len(image), chunk_size), start=1):
            chunk = image[offset : offset + chunk_size]
            await client.write_gatt_char(DFU_PACKET_UUID, chunk, response=False)
            progress.update((offset + len(chunk)) / len(image))
            if packet % _PRN == 0:
                await session.wait_for_receipts(packet // _PRN)
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
        try:
            await session.expect_success(
                _OP_START_DFU, "start", timeout=_LONG_RESPONSE_TIMEOUT
            )
        except EsphomeError:
            with suppress(BleakError, OSError):
                await _write_control(client, _OP_RESET)
            raise

        _LOGGER.debug("DFU: sending the init packet")
        await _write_control(client, _OP_INIT_DFU_PARAMS, 0x00)
        await client.write_gatt_char(DFU_PACKET_UUID, init_packet, response=False)
        await _write_control(client, _OP_INIT_DFU_PARAMS, 0x01)
        await session.expect_success(_OP_INIT_DFU_PARAMS, "init packet")

        await _write_control(
            client, _OP_PACKET_RECEIPT_NOTIF_REQ, *struct.pack("<H", _PRN)
        )
        _LOGGER.debug("DFU: streaming the image")
        await _write_control(client, _OP_RECEIVE_FIRMWARE_IMAGE)
        await _stream_firmware(client, session, image)
        await session.expect_success(
            _OP_RECEIVE_FIRMWARE_IMAGE, "upload", timeout=_LONG_RESPONSE_TIMEOUT
        )

        await _write_control(client, _OP_VALIDATE)
        await session.expect_success(_OP_VALIDATE, "validation")

        # The bootloader resets as soon as it handles this, so the link drops
        # before the write can be acknowledged (an OSError on Windows).
        with suppress(BleakError, OSError):
            await _write_control(client, _OP_ACTIVATE_AND_RESET)
    finally:
        with suppress(Exception):
            await client.stop_notify(DFU_CONTROL_POINT_UUID)


async def _dfu_attempt(address: str, image: bytes, init_packet: bytes) -> None:
    """Run the whole DFU sequence once."""
    async with _connect(address) as client:
        try:
            await _perform_update(client, image, init_packet)
        except (BleakError, OSError) as err:
            raise EsphomeError(f"DFU failed: {err}") from err


async def dfu_upload(address: str, firmware: Path) -> None:
    """Flash ``firmware`` (an Adafruit DFU package) to ``address``."""
    image, init_packet = _load_firmware(firmware)
    for attempt in range(1, _ATTEMPTS + 1):
        _LOGGER.info(
            "Uploading %s to %s (%d bytes)%s...",
            firmware.name,
            address,
            len(image),
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
