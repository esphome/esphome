import asyncio
from collections.abc import Callable, Iterator
from contextlib import contextmanager, nullcontext
from dataclasses import asdict
from functools import partial
from hashlib import sha256
import json
import logging
from pathlib import Path

from bleak.exc import BleakDBusError, BleakDeviceNotFoundError
from smp import header as smpheader
from smp.exceptions import SMPBadStartDelimiter
from smpclient import SMPClient
from smpclient.exceptions import SMPBadSequence
from smpclient.generics import SMPRequest, error, success
from smpclient.mcuboot import IMAGE_TLV, ImageInfo, MCUBootImageError, TLVNotFound
from smpclient.requests.image_management import (
    ImageStatesRead,
    ImageStatesWrite,
    ImageUploadWrite,
)
from smpclient.requests.os_management import ResetWrite
from smpclient.transport import SMPTransportDisconnected
from smpclient.transport.ble import (
    SMPBLETransport,
    SMPBLETransportDeviceNotFound,
    SMPBLETransportException,
)
from smpclient.transport.serial import SMPSerialTransport
from smpclient.transport.udp import SMPUDPTransport

from esphome.core import EsphomeError
from esphome.espota2 import ProgressBar

from .ble_logger import find_device_by_name, is_ble_address
from .const import SMP_UDP_LINK_MTU, SMP_UDP_PORT

BLE_SCAN_TIMEOUT = 10.0  # seconds
RESET_DELAY = 2.0  # seconds to wait before reset, allows on_end action to execute
# Thread is lossy and each chunk waits on a flash write synced with the radio
SMP_UDP_REQUEST_TIMEOUT = 8.0
SMP_UDP_UPLOAD_RETRIES = 15
SMP_UDP_STATE_RETRIES = 3
SMP_UDP_PROBE_TIMEOUT = 2.0
# MCUboot moves the whole image during the swap, then the device rejoins the network
SMP_UDP_REBOOT_WAIT = 60.0

_LOGGER = logging.getLogger(__name__)


class SMPUnreachableError(EsphomeError):
    """The device did not answer over UDP before anything was sent to it."""


class _SequenceMatchingUDPTransport(SMPUDPTransport):
    """Skips late replies to earlier attempts, which stay queued after a timeout."""

    async def send_and_receive(self, data: bytes) -> bytes:
        await self.send(data)
        sequence = smpheader.Header.loads(data[: smpheader.Header.SIZE]).sequence
        while True:
            frame = await self.receive()
            if (
                smpheader.Header.loads(frame[: smpheader.Header.SIZE]).sequence
                == sequence
            ):
                return frame
            _LOGGER.debug("Skipping a late reply to an earlier request")


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
    # No service filter: the SMP UUID only fits in the scan response, which macOS does not filter on.
    # The SMP service is checked on connect.
    device = await find_device_by_name(name, timeout=BLE_SCAN_TIMEOUT)
    if device is None:
        raise EsphomeError(f"BLE device {name} not found")
    return device.address


class _DropTimeoutErrors(logging.Filter):
    """smpclient logs each timed-out request at ERROR, payload included; resends handle them."""

    def filter(self, record: logging.LogRecord) -> bool:
        return not (
            record.levelno == logging.ERROR
            and record.getMessage().startswith("Timeout (")
        )


@contextmanager
def _quiet_smpclient_timeouts() -> Iterator[None]:
    smpclient_logger = logging.getLogger("smpclient")
    log_filter = _DropTimeoutErrors()
    smpclient_logger.addFilter(log_filter)
    try:
        yield
    finally:
        smpclient_logger.removeFilter(log_filter)


async def smpmgr_upload(device: str, firmware: Path, udp: bool = False) -> None:
    try:
        with _quiet_smpclient_timeouts() if udp else nullcontext():
            await _smpmgr_upload(device, firmware, udp)
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


async def _smpmgr_upload(device: str, firmware: Path, udp: bool) -> None:
    image_tlv_sha256 = _get_image_tlv_sha256(firmware)

    if udp:
        smp_client = SMPClient(
            _SequenceMatchingUDPTransport(mtu=SMP_UDP_LINK_MTU),
            device,
            timeout_s=SMP_UDP_REQUEST_TIMEOUT,
        )
    elif is_ble_address(device):
        smp_client = SMPClient(SMPBLETransport(), device)
    else:
        smp_client = SMPClient(SMPSerialTransport(), device)

    _LOGGER.info("Connecting %s...", device)
    try:
        await smp_client.connect()
    except OSError as exc:
        if not udp:
            raise
        raise SMPUnreachableError(f"Cannot reach {device}: {exc}") from exc
    except BleakDeviceNotFoundError as exc:
        raise EsphomeError(f"Device {device} not found") from exc
    except BleakDBusError as exc:
        if "NotPermitted" in exc.dbus_error:
            raise EsphomeError(
                f"Cannot connect to {device}: Make sure the device is paired."
            ) from exc
        raise EsphomeError(f"BLE error connecting to {device}: {exc}") from exc
    except SMPBLETransportException as exc:
        # Reached when a device with the right name has no SMP service, among other causes
        raise EsphomeError(f"Connection error with {device}: {exc}") from exc

    _LOGGER.info("Connected %s...", device)
    try:
        await _smpmgr_upload_connected(
            smp_client, device, firmware, image_tlv_sha256, udp
        )
    finally:
        await smp_client.disconnect()


async def _request_with_retry(
    smp_client: SMPClient,
    make_request: Callable[[], SMPRequest],
    timeout_s: float | None,
    retries: int,
    target: str,
):
    """Send a fresh request (new sequence number) each attempt until one is answered."""
    for attempt in range(retries + 1):
        request = make_request()
        try:
            return await smp_client.request(request, timeout_s)
        except (TimeoutError, SMPBadSequence) as exc:
            if attempt == retries:
                raise EsphomeError(
                    f"{target} could not be reached: no reply to "
                    f"{type(request).__name__} after {retries + 1} attempts"
                ) from exc
            _LOGGER.debug("No reply, resending (%d/%d)", attempt + 1, retries)
    raise AssertionError("unreachable")


async def _image_in_slot(smp_client: SMPClient, image_hash: bytes, target: str) -> bool:
    state = await _request_with_retry(
        smp_client, ImageStatesRead, None, SMP_UDP_STATE_RETRIES, target
    )
    if not success(state):
        _LOGGER.warning(
            "Could not read the image states, sending the image again: %s", state
        )
        return False
    if any(image.hash == image_hash for image in state.images):
        return True
    _LOGGER.warning("The device does not list the image, sending it again")
    return False


async def _udp_upload(
    smp_client: SMPClient,
    image: bytes,
    progress: ProgressBar,
    target: str,
    image_hash: bytes,
) -> None:
    """Upload an image, resending each chunk until the device answers.

    smpclient's upload() gives up on the first lost reply. mcumgr ignores a write at
    an unexpected offset and replies with the offset it wants, so resending is safe.
    """
    total = len(image)
    first = {"len": total, "image": 0, "sha": sha256(image).digest()}
    off = 0
    highest = 0
    stalls = 0
    while off < total:
        fields = first if off == 0 else {}
        _, size = smp_client.get_max_cbor_and_data_size(
            ImageUploadWrite(off=off, data=b"", **fields)
        )
        make_request = partial(
            ImageUploadWrite, off=off, data=image[off : off + size], **fields
        )
        try:
            response = await _request_with_retry(
                smp_client, make_request, None, SMP_UDP_UPLOAD_RETRIES, target
            )
        except EsphomeError as exc:
            raise EsphomeError(f"Upload stalled at offset {off}: {exc}") from exc
        if error(response):
            raise EsphomeError(f"Upload failed at offset {off}: {response}")
        if response.off is None:
            raise EsphomeError(f"No offset received at {off}: {response}")
        if response.off > total:
            raise EsphomeError(f"Device reported offset {response.off} past {total}")
        # mcumgr clears its upload state after the last write, so a resent final chunk
        # whose first reply was lost comes back with offset 0
        if (
            response.off == 0
            and off + size >= total
            and await _image_in_slot(smp_client, image_hash, target)
        ):
            _LOGGER.info(
                "The reply to the last chunk was lost; the device lists the image"
            )
            progress.update(1)
            return
        # Follow the device's offset, so an upload it restarted is sent again. Only a
        # new high resets the count, so stalls and repeated restarts both give up.
        if response.off > highest:
            highest = response.off
            stalls = 0
        elif response.off <= off:
            stalls += 1
            if stalls > SMP_UDP_UPLOAD_RETRIES:
                raise EsphomeError(f"Upload not advancing past offset {highest}")
        off = response.off
        progress.update(off / total)
    # match needs CONFIG_IMG_ENABLE_IMAGE_CHECK, which NCS cannot build with its PSA crypto.
    # Marking the image only finds the slot by its header hash; MCUboot checks the whole
    # image before it boots it.
    if response.match is False:
        raise EsphomeError("Upload failed: device reported mismatched SHA256")
    if response.match is None:
        _LOGGER.info(
            "The device did not verify the image hash; MCUboot checks it at boot"
        )


async def _udp_wait_for_new_image(
    smp_client: SMPClient, device: str, image_hash: bytes
) -> None:
    """Wait for the device to come back running the new image.

    MCUboot boots the old image again when it cannot swap the new one in, and the
    new image then stays pending, so every later upload fails until it is cleared.
    """
    loop = asyncio.get_running_loop()
    deadline = loop.time() + SMP_UDP_REBOOT_WAIT
    state = None
    while loop.time() < deadline:
        # Also skips a reply the old image sends before it reboots
        await asyncio.sleep(SMP_UDP_PROBE_TIMEOUT)
        try:
            reply = await smp_client.request(ImageStatesRead(), SMP_UDP_PROBE_TIMEOUT)
        except (TimeoutError, SMPBadSequence):
            continue
        if not success(reply):
            continue
        state = reply
        if any(image.hash == image_hash and image.active for image in state.images):
            _LOGGER.info("%s runs the new image", device)
            return
    if state is None:
        _LOGGER.warning(
            "%s did not answer after the reboot; the update is not confirmed", device
        )
        return
    raise EsphomeError(
        f"{device} rebooted but did not swap in the new image; it may be too large for "
        f"the MCUboot slot: {state}"
    )


async def _udp_reset(
    smp_client: SMPClient, device: str, image_hash: bytes, target: str
) -> None:
    """Reset the device, resending only when it still runs the old image.

    A blind resend could reboot the new image before it confirms itself, and MCUboot
    would then roll the update back.
    """
    for attempt in range(2):
        try:
            r = await smp_client.request(ResetWrite())
        except (TimeoutError, SMPBadSequence):
            pass
        else:
            if error(r):
                raise EsphomeError(f"Failed to reset {device}: {r}")
            await _udp_wait_for_new_image(smp_client, device, image_hash)
            return
        try:
            state = await _request_with_retry(
                smp_client, ImageStatesRead, SMP_UDP_PROBE_TIMEOUT, 1, target
            )
        except EsphomeError:
            _LOGGER.warning("The reboot of %s is not confirmed", device)
            return
        if not success(state):
            raise EsphomeError(
                f"Could not read the image states of {device} after the reset: {state}"
            )
        ours = next((image for image in state.images if image.hash == image_hash), None)
        if ours is not None and ours.active:
            _LOGGER.info("%s runs the new image", device)
            return
        if ours is None or not ours.pending:
            raise EsphomeError(f"{device} no longer has the new image pending: {state}")
        if attempt:
            raise EsphomeError(f"{device} did not reboot and still runs the old image")
        _LOGGER.info("%s still runs the old image, resending the reset", device)


async def _smpmgr_upload_connected(
    smp_client: SMPClient,
    device: str,
    firmware: Path,
    image_tlv_sha256: bytes,
    udp: bool,
) -> None:
    # UDP requests use the client's timeout; BLE and serial keep their tuned ones
    read_timeout, write_timeout, retries = (
        (None, None, SMP_UDP_STATE_RETRIES) if udp else (2.5, 1.0, 0)
    )
    target = f"{device} on UDP port {SMP_UDP_PORT}" if udp else device
    try:
        image_state = await _request_with_retry(
            smp_client, ImageStatesRead, read_timeout, retries, target
        )
    except SMPBadStartDelimiter as exc:
        raise EsphomeError(f"mcumgr is not supported by device ({device})") from exc
    except EsphomeError as exc:
        if udp:
            raise SMPUnreachableError(str(exc)) from exc
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
        with firmware.open("rb") as file:
            image = file.read()
            upload_size = len(image)
            progress = ProgressBar("Uploading")
            progress.update(0)
            try:
                if udp:
                    await _udp_upload(
                        smp_client, image, progress, target, image_tlv_sha256
                    )
                else:
                    async for offset in smp_client.upload(image):
                        progress.update(offset / upload_size)
            finally:
                progress.done()

    _LOGGER.info("Mark image for testing")
    r = await _request_with_retry(
        smp_client,
        partial(ImageStatesWrite, hash=image_tlv_sha256),
        write_timeout,
        retries,
        target,
    )
    if error(r):
        raise EsphomeError(f"Failed to mark image for testing on {device}: {r}")

    await asyncio.sleep(RESET_DELAY)
    _LOGGER.info("Reset")
    if udp:
        await _udp_reset(smp_client, device, image_tlv_sha256, target)
        return
    r = await smp_client.request(ResetWrite(), write_timeout)

    if error(r):
        raise EsphomeError(f"Failed to reset {device}: {r}")
