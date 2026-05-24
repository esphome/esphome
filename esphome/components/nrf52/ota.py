import asyncio
from dataclasses import asdict
import json
import logging
from pathlib import Path

from bleak import BleakScanner
from bleak.exc import BleakDeviceNotFoundError
from smp.exceptions import SMPBadStartDelimiter
from smpclient import SMPClient
from smpclient.generics import error, success
from smpclient.mcuboot import IMAGE_TLV, ImageInfo, MCUBootImageError, TLVNotFound
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
from smpclient.requests.os_management import ResetWrite
from smpclient.transport import SMPTransportDisconnected
from smpclient.transport.ble import (
    SMPBLETransport,
    SMPBLETransportDeviceNotFound,
    SMPBLETransportException,
)
from smpclient.transport.serial import SMPSerialTransport

from esphome.core import EsphomeError
from esphome.espota2 import ProgressBar

from .ble_logger import is_mac_address

SMP_SERVICE_UUID = "8D53DC1D-1DB7-4CD3-868B-8A527460AA84"
BLE_SCAN_TIMEOUT = 10.0  # seconds
RESET_DELAY = 2.0  # seconds to wait before reset, allows on_end action to execute

_LOGGER = logging.getLogger(__name__)


def _json_state(o: object) -> object:
    """JSON serializer for SMP image state objects."""
    if isinstance(o, (bytes, bytearray)):
        return o.hex()
    if hasattr(o, "hex"):
        return o.hex()
    if hasattr(o, "__dict__"):
        return vars(o)
    return str(o)


async def smpmgr_scan(name: str) -> str:
    _LOGGER.info("Scanning bluetooth for %s...", name)
    for device in await BleakScanner.discover(
        timeout=BLE_SCAN_TIMEOUT, service_uuids=[SMP_SERVICE_UUID]
    ):
        if device.name == name:
            return device.address
    raise EsphomeError(f"BLE device {name} with OTA service not found")


async def smpmgr_upload(device: str, firmware: Path) -> None:
    try:
        await _smpmgr_upload(device, firmware)
    except SMPTransportDisconnected as exc:
        raise EsphomeError(f"{device} was disconnected.") from exc
    except SMPBLETransportDeviceNotFound as exc:
        raise EsphomeError(f"{device} was not found.") from exc


def _get_image_tlv_sha256(file: Path) -> bytes:
    _LOGGER.info("Checking image: %s", str(file))
    try:
        image_info = ImageInfo.load_file(str(file))
        _LOGGER.info(
            "Image header:\n%s", json.dumps(asdict(image_info.header), indent=2)
        )
        _LOGGER.debug(str(image_info))
    except MCUBootImageError as exc:
        raise EsphomeError("Inspection of FW image failed") from exc
    except FileNotFoundError as exc:
        raise EsphomeError(
            f"Firmware image file not found: {file}. Build with zephyr_mcumgr enabled"
        ) from exc

    try:
        image_tlv_sha256 = image_info.get_tlv(IMAGE_TLV.SHA256)
        _LOGGER.info("Image tlv sha256: %s", image_tlv_sha256)
    except TLVNotFound as exc:
        raise EsphomeError("Could not find IMAGE_TLV_SHA256 in image.") from exc
    return image_tlv_sha256.value


async def _smpmgr_upload(device: str, firmware: Path) -> None:
    image_tlv_sha256 = _get_image_tlv_sha256(firmware)

    if is_mac_address(device):
        smp_client = SMPClient(SMPBLETransport(), device)
    else:
        smp_client = SMPClient(SMPSerialTransport(), device)

    _LOGGER.info("Connecting %s...", device)
    try:
        await smp_client.connect()
    except BleakDeviceNotFoundError as exc:
        raise EsphomeError(f"Device {device} not found") from exc
    except SMPBLETransportException as exc:
        raise EsphomeError(f"Connection error with {device}") from exc

    _LOGGER.info("Connected %s...", device)
    try:
        await _smpmgr_upload_connected(smp_client, device, firmware, image_tlv_sha256)
    finally:
        await smp_client.disconnect()


async def _smpmgr_upload_connected(
    smp_client: SMPClient, device: str, firmware: Path, image_tlv_sha256: bytes
) -> None:
    try:
        image_state = await smp_client.request(ImageStatesRead(), 2.5)
    except (SMPBadStartDelimiter, TimeoutError) as exc:
        raise EsphomeError(f"mcumgr is not supported by device ({device})") from exc

    already_uploaded = False

    if error(image_state):
        raise EsphomeError(f"Failed to read image state from {device}: {image_state}")
    if success(image_state):
        if len(image_state.images) == 0:
            _LOGGER.warning("No images on device!")
        for image in image_state.images:
            _LOGGER.info(
                "Image state:\n%s",
                json.dumps(image, indent=2, default=_json_state),
            )
            if image.active and not image.confirmed:
                raise EsphomeError("No free slot. Testing mode but not confirmed yet.")
            if image.hash == image_tlv_sha256:
                if already_uploaded:
                    raise EsphomeError("Both slots have the same image already")
                if image.confirmed:
                    raise EsphomeError("The same image already confirmed")
                _LOGGER.warning("The same image already uploaded")
                already_uploaded = True

    if not already_uploaded:
        with open(firmware, "rb") as file:
            image = file.read()
            upload_size = len(image)
            progress = ProgressBar("Uploading")
            progress.update(0)
            try:
                async for offset in smp_client.upload(image):
                    progress.update(offset / upload_size)
            finally:
                progress.done()

    _LOGGER.info("Mark image for testing")
    r = await smp_client.request(ImageStatesWrite(hash=image_tlv_sha256), 1.0)

    if error(r):
        raise EsphomeError(f"Failed to mark image for testing on {device}: {r}")

    await asyncio.sleep(RESET_DELAY)
    _LOGGER.info("Reset")
    r = await smp_client.request(ResetWrite(), 1.0)

    if error(r):
        raise EsphomeError(f"Failed to reset {device}: {r}")
