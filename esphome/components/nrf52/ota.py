import asyncio
from dataclasses import asdict
from hashlib import sha256
import json
import logging
from pathlib import Path

from bleak import BleakScanner
from bleak.exc import BleakDBusError, BleakDeviceNotFoundError
from smp.exceptions import SMPBadStartDelimiter
from smpclient import SMPClient
from smpclient.exceptions import SMPBadSequence
from smpclient.generics import error, success
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

from .ble_logger import is_mac_address

SMP_SERVICE_UUID = "8D53DC1D-1DB7-4CD3-868B-8A527460AA84"
BLE_SCAN_TIMEOUT = 10.0  # seconds
RESET_DELAY = 2.0  # seconds to wait before reset, allows on_end action to execute
# Cap SMP frames for UDP to fit the IPv6 minimum MTU (1280) less the IPv6 (40)
# and UDP (8) headers. Thread links use the 1280-byte minimum, and the host kernel rejects larger datagrams with EMSGSIZE. smpclient defaults to 1500.
SMP_UDP_MTU = 1232
# UDP over Thread is lossy and high-latency, and every upload chunk triggers a
# flash write that is synchronized with the 802.15.4 radio (MPSL flash sync), so
# responses can stall past smpclient's defaults (2.5s). smpclient.upload() also
# aborts on the first timeout with no retransmission, which a lossy mesh will not
# survive over hundreds of chunks. On the UDP path we drive the upload ourselves
# and resend each chunk until it is acknowledged (mcumgr image upload is
# resumable and offset writes are idempotent, so resending is safe).
SMP_NET_REQUEST_TIMEOUT = 20.0
SMP_NET_UPLOAD_FIRST_TIMEOUT = 60.0  # first write triggers the secondary-slot erase
SMP_NET_UPLOAD_TIMEOUT = 8.0  # per-chunk; retried on timeout
SMP_NET_UPLOAD_RETRIES = 15

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

    from esphome.upload_targets import PortType, get_port_type

    is_udp = False
    if is_mac_address(device):
        smp_client = SMPClient(SMPBLETransport(), device)
    elif get_port_type(device) == PortType.NETWORK:
        # IP/hostname target (e.g. over Thread): SMP over UDP, default port 1337.
        is_udp = True
        smp_client = SMPClient(SMPUDPTransport(mtu=SMP_UDP_MTU), device)
    else:
        smp_client = SMPClient(SMPSerialTransport(), device)

    _LOGGER.info("Connecting %s...", device)
    try:
        # connect() also covers name resolution; mDNS (.local) over a Thread border
        # router can take longer than smpclient's 5s default, so give it headroom.
        await smp_client.connect(
            connect_timeout_s=SMP_NET_REQUEST_TIMEOUT if is_udp else 5.0
        )
    except BleakDeviceNotFoundError as exc:
        raise EsphomeError(f"Device {device} not found") from exc
    except BleakDBusError as exc:
        if "NotPermitted" in exc.dbus_error:
            raise EsphomeError(
                f"Cannot connect to {device}: Make sure the device is paired."
            ) from exc
        raise EsphomeError(f"BLE error connecting to {device}: {exc}") from exc
    except SMPBLETransportException as exc:
        raise EsphomeError(f"Connection error with {device}") from exc

    _LOGGER.info("Connected %s...", device)
    try:
        await _smpmgr_upload_connected(
            smp_client, device, firmware, image_tlv_sha256, is_udp
        )
    finally:
        await smp_client.disconnect()


async def _upload_chunk_with_resume(
    smp_client: SMPClient, image: bytes, off: int, image_sha: bytes
):
    """Send the image chunk at ``off`` and return the SMP response.

    Resends the chunk on a lost ack until the device acknowledges it. Offset writes
    are idempotent in mcumgr: if a response was merely lost, the device replies with
    its current offset without rewriting, so resending always converges. A genuine
    transport disconnect is NOT retried here (it would burn all retries instantly);
    it propagates to ``smpmgr_upload`` which reports the real cause.
    """
    total = len(image)
    timeout = SMP_NET_UPLOAD_FIRST_TIMEOUT if off == 0 else SMP_NET_UPLOAD_TIMEOUT
    for attempt in range(SMP_NET_UPLOAD_RETRIES):
        # NOTE: _maximize_image_upload_write_packet is smpclient-internal (no public
        # chunk-sizing helper exists). _resilient_upload guards its presence up front,
        # so a future smpclient rename fails with a clear error there rather than a
        # cryptic AttributeError here. The serial/BLE paths are unaffected.
        request = smp_client._maximize_image_upload_write_packet(  # pylint: disable=protected-access
            ImageUploadWrite(
                off=off,
                data=b"",
                len=total if off == 0 else None,
                image=0 if off == 0 else None,
                sha=image_sha if off == 0 else None,
            ),
            image,
        )
        try:
            response = await smp_client.request(request, timeout_s=timeout)
        except (TimeoutError, SMPBadSequence):
            # SMPBadSequence: a *stalled* (not lost) response for a previous attempt
            # arrived out of order (each request has a fresh sequence number, and the
            # UDP transport doesn't drain stale datagrams). Resending discards it; the
            # idempotent offset write converges. This is the high-latency Thread case
            # the resend logic exists for, so it must be retried, not fatal.
            _LOGGER.debug(
                "No/stale ack at offset %d, resending (%d/%d)",
                off,
                attempt + 1,
                SMP_NET_UPLOAD_RETRIES,
            )
            continue
        if error(response):
            raise EsphomeError(f"Upload failed at offset {off}: {response}")
        if response.off is None:
            raise EsphomeError(f"No offset received at {off}: {response}")
        return response
    raise EsphomeError(
        f"Upload stalled at offset {off} after {SMP_NET_UPLOAD_RETRIES} retries"
    )


async def _request_with_retry(
    smp_client: SMPClient, request, timeout_s: float, retries: int
):
    """Issue a one-shot SMP request, resending it on timeout (lossy UDP path)."""
    for attempt in range(retries + 1):
        try:
            return await smp_client.request(request, timeout_s)
        except (TimeoutError, SMPBadSequence) as exc:
            # See _upload_chunk_with_resume: a stalled out-of-order reply raises
            # SMPBadSequence; resend to discard it.
            if attempt >= retries:
                raise EsphomeError(
                    f"SMP request timed out after {retries + 1} attempts"
                ) from exc
            _LOGGER.debug("Request timed out, resending (%d/%d)", attempt + 1, retries)
    # Unreachable: range(retries + 1) always runs at least once and that iteration
    # either returns or raises. Present to satisfy the "explicit return" linter.
    raise EsphomeError("SMP request retry loop exited unexpectedly")


async def _resilient_upload(
    smp_client: SMPClient, image: bytes, progress: ProgressBar
) -> None:
    """Upload an image over a lossy transport (UDP), resending dropped chunks."""
    # _upload_chunk_with_resume relies on smpclient's internal chunk-sizing helper
    # (no public equivalent exists). Guard its presence up front so a future smpclient
    # rename fails with an actionable message instead of a cryptic AttributeError
    # partway through an upload.
    if not hasattr(smp_client, "_maximize_image_upload_write_packet"):
        raise EsphomeError(
            "Installed smpclient is missing the internal helper "
            "'_maximize_image_upload_write_packet' required for UDP OTA chunk sizing. "
            "Pin a compatible smpclient version or report this upstream."
        )
    total = len(image)
    image_sha = sha256(image).digest()  # constant — hoisted out of the retry loop
    off = 0
    stalls = 0
    response = None
    while off < total:
        response = await _upload_chunk_with_resume(smp_client, image, off, image_sha)
        # Guard against a device that never advances (e.g. repeats the same offset).
        if response.off <= off:
            stalls += 1
            if stalls > SMP_NET_UPLOAD_RETRIES:
                raise EsphomeError(f"Upload not advancing past offset {off}")
        else:
            stalls = 0
        # never move backward (a stale/odd reply can't rewind progress)
        off = max(off, response.off)
        progress.update(min(off, total) / total)
    # smpclient.upload() verifies the device-reported SHA match on the final write;
    # replicate that. Best-effort: the device sets `match` only on the full-image
    # verification, so a *resent* final ack carries match=None and is skipped here —
    # a real mismatch is still caught by the ImageStatesWrite(hash=...) mark below
    # (no slot will hold that hash).
    if response is not None and getattr(response, "match", None) is False:
        raise EsphomeError("Upload failed: device reported mismatched SHA256")


async def _smpmgr_upload_connected(
    smp_client: SMPClient,
    device: str,
    firmware: Path,
    image_tlv_sha256: bytes,
    is_udp: bool,
) -> None:
    # is_udp is decided once in _smpmgr_upload (after is_mac_address), so a BLE/MAC
    # device is never misclassified as UDP here (get_port_type() is a catch-all).
    req_timeout = SMP_NET_REQUEST_TIMEOUT if is_udp else 2.5

    try:
        image_state = await smp_client.request(ImageStatesRead(), req_timeout)
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
        with firmware.open("rb") as file:
            image = file.read()
            upload_size = len(image)
            progress = ProgressBar("Uploading")
            progress.update(0)
            try:
                if is_udp:
                    await _resilient_upload(smp_client, image, progress)
                else:
                    async for offset in smp_client.upload(image):
                        progress.update(offset / upload_size)
            finally:
                progress.done()

    _LOGGER.info("Mark image for testing")
    # On the lossy UDP path this single request can be dropped; resend it on timeout.
    r = await _request_with_retry(
        smp_client,
        ImageStatesWrite(hash=image_tlv_sha256),
        req_timeout if is_udp else 1.0,
        SMP_NET_UPLOAD_RETRIES if is_udp else 0,
    )
    if error(r):
        raise EsphomeError(f"Failed to mark image for testing on {device}: {r}")

    await asyncio.sleep(RESET_DELAY)
    _LOGGER.info("Reset")
    try:
        r = await smp_client.request(ResetWrite(), req_timeout if is_udp else 1.0)
    except (TimeoutError, SMPTransportDisconnected):
        # On the lossy UDP path the device reboots and its reset response is routinely
        # lost — treat that as success. For BLE/serial keep the original behavior (let
        # it raise) so a reset that never reached the device isn't a false success.
        if not is_udp:
            raise
        _LOGGER.info("No reset response (device already rebooting) - OK")
        return

    if error(r):
        raise EsphomeError(f"Failed to reset {device}: {r}")
