"""SMP over UDP for nRF52 OTA: the lossy-link pieces of the MCUmgr upload."""

import asyncio
from collections.abc import Iterator
from contextlib import contextmanager
from functools import partial
from hashlib import sha256
import logging

from smp import header as smpheader
from smpclient import SMPClient
from smpclient.generics import error, success
from smpclient.requests.image_management import ImageStatesRead, ImageUploadWrite
from smpclient.requests.os_management import ResetWrite
from smpclient.transport.udp import SMPUDPTransport

from esphome.core import EsphomeError
from esphome.espota2 import ProgressBar

from .ota import _find_image, _request_with_retry, _try_request

# Thread is lossy and each chunk waits on a flash write synced with the radio
SMP_UDP_REQUEST_TIMEOUT = 8.0
SMP_UDP_UPLOAD_RETRIES = 15
SMP_UDP_STATE_RETRIES = 3
SMP_UDP_PROBE_TIMEOUT = 2.0
# MCUboot moves the whole image during the swap, then the device rejoins the network
SMP_UDP_REBOOT_WAIT = 60.0
# After a lost reset reply: two probes tell an old image still running from a reboot
SMP_UDP_LOST_RESET_WAIT = 2 * SMP_UDP_PROBE_TIMEOUT
SMP_UDP_RESET_ATTEMPTS = 2  # the reset and at most one resend

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


def _drop_timeout_errors(record: logging.LogRecord) -> bool:
    """Drop smpclient's ERROR record, payload included, for each timed-out request."""
    return not (
        record.levelno == logging.ERROR and record.getMessage().startswith("Timeout (")
    )


@contextmanager
def _quiet_smpclient_timeouts(enabled: bool) -> Iterator[None]:
    if not enabled:
        yield
        return
    smpclient_logger = logging.getLogger("smpclient")
    smpclient_logger.addFilter(_drop_timeout_errors)
    try:
        yield
    finally:
        smpclient_logger.removeFilter(_drop_timeout_errors)


async def _image_in_slot(smp_client: SMPClient, image_hash: bytes, target: str) -> bool:
    state = await _request_with_retry(
        smp_client, ImageStatesRead, None, SMP_UDP_STATE_RETRIES, target
    )
    if not success(state):
        _LOGGER.warning(
            "Could not read the image states, sending the image again: %s", state
        )
        return False
    if _find_image(state, image_hash) is not None:
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
                smp_client,
                make_request,
                SMP_UDP_REQUEST_TIMEOUT,
                SMP_UDP_UPLOAD_RETRIES,
                target,
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


async def _poll_for_new_image(
    smp_client: SMPClient, image_hash: bytes, wait_s: float
) -> object | None:
    """Poll the image states after a reset and return the last reply, None if silent.

    Stops at the new image, at the first good reply after a silence (the device
    rebooted), or after wait_s.
    """
    # Also skips a reply the old image sends before it reboots
    await asyncio.sleep(SMP_UDP_PROBE_TIMEOUT)
    loop = asyncio.get_running_loop()
    deadline = loop.time() + wait_s
    state = None
    rebooted = False
    while loop.time() < deadline:
        reply = await _try_request(smp_client, ImageStatesRead(), SMP_UDP_PROBE_TIMEOUT)
        if reply is None:
            rebooted = True
            continue
        state = reply
        if success(reply) and (
            rebooted or getattr(_find_image(reply, image_hash), "active", False)
        ):
            break
    return state


async def _udp_reset(smp_client: SMPClient, device: str, image_hash: bytes) -> None:
    """Reset the device and check that it comes back running the new image.

    Resends only when the old image still runs: a blind resend could reboot the new
    image before it confirms itself, and MCUboot would roll the update back. MCUboot
    boots the old image again when it cannot swap the new one in, and the new image
    then stays pending, so every later upload fails until the slot is cleared.
    """
    for attempt in range(SMP_UDP_RESET_ATTEMPTS):
        reply = await _try_request(smp_client, ResetWrite(), SMP_UDP_REQUEST_TIMEOUT)
        if reply is not None and error(reply):
            raise EsphomeError(f"Failed to reset {device}: {reply}")
        state = await _poll_for_new_image(
            smp_client,
            image_hash,
            SMP_UDP_REBOOT_WAIT if reply is not None else SMP_UDP_LOST_RESET_WAIT,
        )
        if state is None:
            # Silence after a reset most likely means it is still swapping or rejoining
            _LOGGER.warning(
                "%s did not answer after the reset; check that it runs the new image",
                device,
            )
            return
        if not success(state):
            raise EsphomeError(
                f"Could not read the image states of {device} after the reset, it "
                f"answered with an error: {state}"
            )
        ours = _find_image(state, image_hash)
        if ours is not None and ours.active:
            _LOGGER.info("%s runs the new image", device)
            return
        if reply is not None:
            raise EsphomeError(
                f"{device} rebooted but did not swap in the new image; it may be too "
                f"large for the MCUboot slot: {state}"
            )
        if ours is None or not ours.pending:
            raise EsphomeError(f"{device} no longer has the new image pending: {state}")
        if attempt == SMP_UDP_RESET_ATTEMPTS - 1:
            raise EsphomeError(
                f"{device} still runs the old image: it did not reboot, or could not "
                "swap in the new image (it may be too large for the MCUboot slot)"
            )
        # Resetting the old image is harmless, even if it rebooted and could not swap
        _LOGGER.info("%s still runs the old image, resending the reset", device)
