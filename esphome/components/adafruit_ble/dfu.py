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
from dataclasses import dataclass
import json
import logging
from pathlib import Path
import struct
import zipfile

from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData
from bleak.exc import (
    BleakDBusError,
    BleakDeviceNotFoundError,
    BleakError,
    BleakGATTProtocolError,
    BleakGATTProtocolErrorCode,
)

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
_RESPONSE_INVALID_STATE = 0x02
_RESPONSE_STATUS_NAMES = {
    0x02: "invalid state",
    0x03: "not supported",
    0x04: "data size exceeds limit",
    0x05: "CRC error",
    0x06: "operation failed",
}

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
# The bootloader rejects image sizes and data packets that are not whole words.
_WORD_SIZE = 4
_SCAN_TIMEOUT = 10.0
_RESPONSE_TIMEOUT = 30.0
# Starting the update erases flash and finishing it writes the last pages.
_LONG_RESPONSE_TIMEOUT = 60.0
_RECEIPT_TIMEOUT = 2.0
# Time the bootloader needs to start advertising after the DFU request.
_REBOOT_TIMEOUT = 20.0
# Without MAC addresses (macOS), the bootloader is the one DFU device that was
# not there before the request: the scan that lists those before it, and how
# long to keep watching for a second new one after the first appears.
_DFU_SNAPSHOT_TIMEOUT = 3.0
_NEW_DEVICE_SETTLE = 3.0
# macOS reports a device once per scan session, so a fresh session is needed to
# see whether the application still advertises.
_APP_CHECK_TIMEOUT = 3.0
# Error responses to the DFU mode request that pairing the device would fix.
_PAIRING_ERRORS = {
    BleakGATTProtocolErrorCode.INSUFFICIENT_AUTHENTICATION,
    BleakGATTProtocolErrorCode.INSUFFICIENT_AUTHORIZATION,
    BleakGATTProtocolErrorCode.INSUFFICIENT_ENCRYPTION,
    BleakGATTProtocolErrorCode.INSUFFICIENT_ENCRYPTION_KEY_SIZE,
}

BOOTLOADER_HINT = (
    "A device left in its bootloader by a failed upload advertises at its MAC "
    "address + 1; pass that address with --device (on macOS, the UUID of the "
    "DFU device)."
)


class _DfuRejectedError(EsphomeError):
    """The bootloader answered with a failure status; retrying will not help."""


class _DfuStaleStateError(_DfuRejectedError):
    """START found the bootloader mid-update from an interrupted transfer.

    The failed START resets the bootloader, so one more attempt can succeed.
    """


def _rejected_error(request_op: int, step: str, status: int) -> _DfuRejectedError:
    name = _RESPONSE_STATUS_NAMES.get(status, "unknown")
    error = (
        _DfuStaleStateError
        if request_op == _OP_START_DFU and status == _RESPONSE_INVALID_STATE
        else _DfuRejectedError
    )
    return error(
        f"DFU {step} (opcode {request_op:#04x}) rejected by the bootloader "
        f"with status {status:#04x} ({name})"
    )


class _DfuSession:
    """Control point notifications and packet receipts of one DFU run."""

    def __init__(self) -> None:
        self._responses: asyncio.Queue[tuple[int, int]] = asyncio.Queue()
        self._receipt = asyncio.Event()
        self._rejection: tuple[int, int] | None = None
        self.receipts = 0

    def notification_handler(self, _sender: int, data: bytearray) -> None:
        if not data:
            _LOGGER.debug("DFU: ignoring an empty notification")
            return
        opcode = data[0]
        if opcode == _OP_RESPONSE_CODE and len(data) >= 3:
            self._responses.put_nowait((data[1], data[2]))
            if data[2] != _RESPONSE_SUCCESS:
                # Wakes a receipt wait, which no receipt would end after a rejection
                self._rejection = (data[1], data[2])
                self._receipt.set()
        elif opcode == _OP_PACKET_RECEIPT_NOTIF:
            self.receipts += 1
            self._receipt.set()
        else:
            _LOGGER.debug("DFU: ignoring notification %s", data.hex())

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
            raise _rejected_error(request_op, step, status)

    def raise_if_rejected(self, step: str) -> None:
        """Raise if the bootloader already answered a command with a failure."""
        if self._rejection is not None:
            request_op, status = self._rejection
            raise _rejected_error(request_op, step, status)

    async def wait_for_receipts(self, count: int, step: str) -> None:
        """Wait until ``count`` packet receipts have arrived in total."""
        try:
            async with asyncio.timeout(_RECEIPT_TIMEOUT):
                while self.receipts < count:
                    self.raise_if_rejected(step)
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


async def _dfu_devices() -> set[str]:
    """Addresses of the devices advertising the DFU service right now."""
    found = await BleakScanner.discover(timeout=_DFU_SNAPSHOT_TIMEOUT, return_adv=True)
    return {
        device.address
        for device, advertisement in found.values()
        if DFU_SERVICE_UUID in _service_uuids(advertisement)
    }


@dataclass(frozen=True)
class DfuPackage:
    """The application image and init packet of an Adafruit DFU package."""

    name: str
    image: bytes
    init_packet: bytes


def load_firmware(firmware: Path) -> DfuPackage:
    """Read and check a DFU package, before the device is asked to reboot."""
    if not firmware.is_file():
        raise EsphomeError(
            f"DFU package not found: {firmware}. Compile the firmware first."
        )
    try:
        return _read_package(firmware)
    except (zipfile.BadZipFile, KeyError, ValueError) as err:
        raise EsphomeError(f"Invalid DFU package {firmware}: {err}") from err


def _read_package(firmware: Path) -> DfuPackage:
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
    if len(image) % _WORD_SIZE:
        raise EsphomeError(
            f"The application image in {firmware} is {len(image)} bytes, "
            f"which the bootloader rejects as it is not a multiple of {_WORD_SIZE}"
        )
    return DfuPackage(firmware.name, image, init_packet)


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
    request_error: Exception | None = None
    known_dfu: set[str] | None = None
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
            if not MAC_ADDRESS_PATTERN.match(address):
                # macOS hides MAC addresses and gives the bootloader its own UUID
                known_dfu = await _dfu_devices()
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
                # The error is only reported if the bootloader then does not show
                # up, except an error response on macOS (find_dfu_bootloader).
                _LOGGER.debug("DFU mode request ended with %s", err)
                request_error = err
    except _DeviceNotFoundError:
        _LOGGER.info("%s was not found, looking for its DFU bootloader", address)
    return await find_dfu_bootloader(address, request_error, known_dfu)


async def _find_new_dfu_devices(
    known_dfu: set[str], app_address: str
) -> tuple[list[str], bool]:
    """Scan for DFU devices that are not in ``known_dfu``.

    Also reports whether ``app_address`` still advertised after one appeared: an application that really restarted
    into its bootloader stops advertising, so a new DFU device seen while it keeps going belongs to another board.
    """
    known = {address.lower() for address in known_dfu}
    app = app_address.lower()
    new: dict[str, str] = {}
    appeared = asyncio.Event()
    app_still_advertising = False

    def _on_advertisement(device: BLEDevice, advertisement: AdvertisementData) -> None:
        nonlocal app_still_advertising
        key = device.address.lower()
        if key == app:
            app_still_advertising = app_still_advertising or appeared.is_set()
            return
        if DFU_SERVICE_UUID in _service_uuids(advertisement) and key not in known:
            new.setdefault(key, device.address)
            appeared.set()

    async with BleakScanner(detection_callback=_on_advertisement):
        with suppress(TimeoutError):
            await asyncio.wait_for(appeared.wait(), _REBOOT_TIMEOUT)
        if appeared.is_set():
            await asyncio.sleep(_NEW_DEVICE_SETTLE)
    return list(new.values()), app_still_advertising


async def _is_advertising(address: str) -> bool:
    """Whether ``address`` shows up in a fresh scan session."""
    wanted = address.lower()
    device = await BleakScanner.find_device_by_filter(
        lambda device, _advertisement: device.address.lower() == wanted,
        timeout=_APP_CHECK_TIMEOUT,
    )
    return device is not None


def _raise_if_request_rejected(address: str, request_error: Exception | None) -> None:
    """Raise if the application answered the DFU mode request with an error.

    An error response comes from the running application, so it did not
    restart. A dropped link stays acceptable, as the reboot can look like that.
    """
    if not isinstance(request_error, BleakGATTProtocolError):
        return
    message = f"{address} rejected the DFU mode request: {request_error.args[-1]}."
    if request_error.code in _PAIRING_ERRORS:
        message += " Make sure the device is paired."
    raise EsphomeError(message) from request_error


async def find_dfu_bootloader(
    address: str,
    request_error: Exception | None = None,
    known_dfu: set[str] | None = None,
) -> str:
    """Scan for the bootloader of the device that was asked to enter DFU mode.

    Only ``address`` and the address above it are accepted, so a different
    device that happens to be in DFU mode nearby is never flashed. With
    ``known_dfu`` (the DFU devices seen before the request, for hosts without
    MAC addresses) the one DFU device that is new is accepted instead, and only
    if exactly one appeared, the application did not reject the request and it
    no longer advertises.
    """
    _LOGGER.info("Scanning for the DFU bootloader...")
    if known_dfu is not None:
        _raise_if_request_rejected(address, request_error)
        new, app_still_advertising = await _find_new_dfu_devices(known_dfu, address)
        if len(new) == 1 and not app_still_advertising:
            app_still_advertising = await _is_advertising(address)
            if not app_still_advertising:
                return new[0]
        if app_still_advertising:
            _LOGGER.warning(
                "%s kept advertising, so it did not restart into its bootloader",
                address,
            )
        raise _not_found_error(address, request_error, known_dfu | set(new))
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
    raise _not_found_error(address, request_error, others)


def _not_found_error(
    address: str, request_error: Exception | None, others: set[str]
) -> EsphomeError:
    message = f"The DFU bootloader of {address} was not found."
    if request_error is not None:
        message += f" The DFU mode request failed: {request_error}."
    if others:
        message += f" Other devices in DFU mode: {', '.join(sorted(others))}."
    return EsphomeError(f"{message} {BOOTLOADER_HINT}")


async def _write_control(client: BleakClient, op: int, *payload: int) -> None:
    await client.write_gatt_char(
        DFU_CONTROL_POINT_UUID, bytes([op, *payload]), response=True
    )


async def _stream_firmware(
    client: BleakClient, session: _DfuSession, image: bytes
) -> None:
    # Every data packet must be whole words; the image size is too, so the last
    # packet is as well.
    chunk_size = max(20, min(client.mtu_size - 3, _MAX_CHUNK_SIZE))
    chunk_size -= chunk_size % _WORD_SIZE
    _LOGGER.debug("DFU uses %d byte chunks for %d bytes", chunk_size, len(image))
    progress = ProgressBar("Uploading")
    progress.update(0)
    try:
        for packet, offset in enumerate(range(0, len(image), chunk_size), start=1):
            session.raise_if_rejected("upload")
            chunk = image[offset : offset + chunk_size]
            await client.write_gatt_char(DFU_PACKET_UUID, chunk, response=False)
            progress.update((offset + len(chunk)) / len(image))
            # The packet that completes the image is answered with the
            # RECEIVE_FIRMWARE_IMAGE response instead of a receipt.
            if packet % _PRN == 0 and offset + len(chunk) < len(image):
                await session.wait_for_receipts(packet // _PRN, "upload")
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
        # before the write can be acknowledged (an OSError on Windows). An error
        # response means it answered and refused.
        try:
            await _write_control(client, _OP_ACTIVATE_AND_RESET)
        except BleakGATTProtocolError as err:
            raise _DfuRejectedError(
                f"DFU activation rejected by the bootloader: {err}"
            ) from err
        except (BleakError, OSError) as err:
            _LOGGER.debug("DFU activation ended with %s", err)
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


async def dfu_upload(address: str, package: DfuPackage) -> None:
    """Flash ``package`` (from ``load_firmware``) to ``address``."""
    stale_state_retried = False
    for attempt in range(1, _ATTEMPTS + 1):
        _LOGGER.info(
            "Uploading %s to %s (%d bytes)%s...",
            package.name,
            address,
            len(package.image),
            f", attempt {attempt}/{_ATTEMPTS}" if attempt > 1 else "",
        )
        try:
            await _dfu_attempt(address, package.image, package.init_packet)
        except _DfuStaleStateError as err:
            if stale_state_retried or attempt == _ATTEMPTS:
                raise
            stale_state_retried = True
            _LOGGER.warning(
                "%s; it was left mid-update and has been reset, retrying", err
            )
            await asyncio.sleep(_RETRY_DELAY)
        except _DfuRejectedError:
            raise
        except EsphomeError as err:
            if attempt == _ATTEMPTS:
                raise
            _LOGGER.warning("DFU attempt %d/%d failed: %s", attempt, _ATTEMPTS, err)
            await asyncio.sleep(_RETRY_DELAY)
        else:
            _LOGGER.info("DFU finished; the device restarts into the new firmware")
            return
