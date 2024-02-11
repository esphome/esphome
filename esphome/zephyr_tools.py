import asyncio
import logging
import re
from typing import Final
from rich.pretty import pprint
from bleak import BleakScanner
from bleak.exc import BleakDeviceNotFoundError
from smpclient.transport.ble import SMPBLETransport
from smpclient.transport.serial import SMPSerialTransport
from smpclient import SMPClient
from smpclient.mcuboot import IMAGE_TLV, ImageInfo, TLVNotFound
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
from smpclient.requests.os_management import ResetWrite
from pyocd.tools.lists import ListGenerator

from smpclient.generics import error, success
from esphome.espota2 import ProgressBar

SMP_SERVICE_UUID = "8D53DC1D-1DB7-4CD3-868B-8A527460AA84"
MAC_ADDRESS_PATTERN: Final = re.compile(
    r"([0-9A-F]{2}[:]){5}[0-9A-F]{2}$", flags=re.IGNORECASE
)

_LOGGER = logging.getLogger(__name__)


async def smpmgr_scan():
    _LOGGER.info("Scanning bluetooth...")
    devices = await BleakScanner.discover(service_uuids=[SMP_SERVICE_UUID])
    return devices


def get_image_tlv_sha256(file):
    _LOGGER.info(f"Checking image: {str(file)}")
    try:
        image_info = ImageInfo.load_file(str(file))
        pprint(image_info.header)
        _LOGGER.debug(str(image_info))
    except Exception as e:
        _LOGGER.error(f"Inspection of FW image failed: {e}")
        return None

    try:
        image_tlv_sha256 = image_info.get_tlv(IMAGE_TLV.SHA256)
        _LOGGER.debug(f"IMAGE_TLV_SHA256: {image_tlv_sha256}")
    except TLVNotFound:
        _LOGGER.error("Could not find IMAGE_TLV_SHA256 in image.")
        return None
    return image_tlv_sha256.value


async def smpmgr_upload(config, host, firmware):
    image_tlv_sha256 = get_image_tlv_sha256(firmware)
    if image_tlv_sha256 is None:
        return 1

    if MAC_ADDRESS_PATTERN.match(host):
        smp_client = SMPClient(SMPBLETransport(), host)
    else:
        smp_client = SMPClient(SMPSerialTransport(mtu=256), host)

    _LOGGER.info(f"Connecting {host}...")
    try:
        await smp_client.connect()
    except BleakDeviceNotFoundError:
        _LOGGER.error(f"Device {host} not found")
        return 1

    _LOGGER.info(f"Connected {host}...")

    image_state = await asyncio.wait_for(
        smp_client.request(ImageStatesRead()), timeout=SMPClient.MEDIUM_TIMEOUT
    )

    already_uploaded = False

    if error(image_state):
        _LOGGER.error(image_state)
        return 1
    elif success(image_state):
        if len(image_state.images) == 0:
            _LOGGER.warning("No images on device!")
        for image in image_state.images:
            pprint(image)
            if image.hash == image_tlv_sha256:
                if already_uploaded:
                    _LOGGER.error("Both slots have the same image")
                    return 1
                else:
                    if image.confirmed:
                        _LOGGER.error("Image already confirmted")
                        return 1
                    _LOGGER.warning("The same image already uploaded")
                    already_uploaded = True

    if not already_uploaded:
        with open(firmware, "rb") as file:
            image = file.read()
            file.close()
            upload_size = len(image)
            progress = ProgressBar()
            progress.update(0)
            async for offset in smp_client.upload(image):
                progress.update(offset / upload_size)
            progress.done()

    _LOGGER.info("Mark image for testing")
    r = await asyncio.wait_for(
        smp_client.request(ImageStatesWrite(hash=image_tlv_sha256)),
        timeout=SMPClient.SHORT_TIMEOUT,
    )

    if error(r):
        _LOGGER.error(r)
        return 1

    _LOGGER.info("Reset")
    r = await asyncio.wait_for(
        smp_client.request(ResetWrite()), timeout=SMPClient.SHORT_TIMEOUT
    )

    if error(r):
        _LOGGER.error(r)
        return 1

    return 0


def list_pyocd():
    return ListGenerator.list_probes()["boards"]
