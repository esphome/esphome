from __future__ import annotations

from collections.abc import Sequence
from contextlib import suppress
import gzip
import hashlib
import logging
import random
import selectors
import socket
import sys
import time

from esphome.core import EsphomeError
from esphome.helpers import resolve_ip_address

RESPONSE_OK = 0x00
RESPONSE_REQUEST_AUTH = 0x01

RESPONSE_HEADER_OK = 0x40
RESPONSE_AUTH_OK = 0x41
RESPONSE_UPDATE_PREPARE_OK = 0x42
RESPONSE_BIN_MD5_OK = 0x43
RESPONSE_RECEIVE_OK = 0x44
RESPONSE_UPDATE_END_OK = 0x45
RESPONSE_SUPPORTS_COMPRESSION = 0x46
RESPONSE_CHUNK_OK = 0x47

RESPONSE_ERROR_MAGIC = 0x80
RESPONSE_ERROR_UPDATE_PREPARE = 0x81
RESPONSE_ERROR_AUTH_INVALID = 0x82
RESPONSE_ERROR_WRITING_FLASH = 0x83
RESPONSE_ERROR_UPDATE_END = 0x84
RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 0x85
RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG = 0x86
RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG = 0x87
RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE = 0x88
RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE = 0x89
RESPONSE_ERROR_NO_UPDATE_PARTITION = 0x8A
RESPONSE_ERROR_MD5_MISMATCH = 0x8B
RESPONSE_ERROR_UNKNOWN = 0xFF

OTA_VERSION_1_0 = 1
OTA_VERSION_2_0 = 2

MAGIC_BYTES = [0x6C, 0x26, 0xF7, 0x5C, 0x45]

FEATURE_SUPPORTS_COMPRESSION = 0x01

OTA_IMAGE_ACK_SIZE_OTA_V2 = 8192
OTA_IMAGE_TRANSFER_TIMEOUT = 600.0
OTA_CONNECT_TIMEOUT = 60.0
PROGRESS_UPDATE_THROTTLE_SEC = 0.1
PROGRESS_TIMER_FORCE_SEC = 0.3

_LOGGER = logging.getLogger(__name__)


class ProgressBar:
    """Render upload progress to stderr."""

    def __init__(self) -> None:
        self._finished = False
        self._last_percent_int: int | None = None
        self._last_segments: tuple[int | None, int | None] | None = None

    def update(
        self,
        progress: float,
        *,
        acked_segments: int | None = None,
        total_segments: int | None = None,
    ) -> None:
        """Render the bar at the current progress ratio.

        Args:
            progress: Ratio of bytes transferred, between 0 and 1.
            acked_segments: Segments acknowledged by the device. If omitted,
                only the percent bar is shown.
            total_segments: Total number of segments to transfer. Provide this
                together with ``acked_segments`` to display the segment suffix.
        """
        if self._finished:
            return

        p = 1.0 if progress >= 1 else max(0.0, float(progress))
        percent_int = int(round(p * 10000))
        seg_pair = (acked_segments, total_segments)
        if percent_int == self._last_percent_int and seg_pair == self._last_segments:
            return

        bar_length = 60
        status = ""
        if p >= 1:
            status = "Done...\r\n"
            self._finished = True
        block = int(round(bar_length * (percent_int / 10000.0)))
        if acked_segments is not None and total_segments is not None:
            width = len(str(total_segments))
            seg_text = f" - Segments: {acked_segments:>{width}}/{total_segments}"
            percent = f"{round(p * 100, 2):6.2f} %"
        else:
            seg_text = ""
            percent = f"{round(p * 100, 2):6.2f}%"
        text = (
            f"\rUploading: [{'=' * block + ' ' * (bar_length - block)}] "
            f"{percent}{seg_text} {status}"
        )
        sys.stderr.write(text)
        sys.stderr.flush()
        self._last_percent_int = percent_int
        self._last_segments = seg_pair

    def get_last_percent_int(self) -> int | None:
        """Return the most recent percent as an integer."""
        return self._last_percent_int

    def get_last_segments(self) -> tuple[int | None, int | None] | None:
        """Return the most recent segment pair."""
        return self._last_segments

    def done(self) -> None:
        """Finish the bar with a trailing newline."""
        self._finished = True
        sys.stderr.write("\n")
        sys.stderr.flush()


class OTAError(EsphomeError):
    """Exception raised for OTA update errors."""


def recv_decode(sock: socket.socket, amount: int) -> bytes:
    """Receive bytes from the socket.

    Args:
        sock: Socket to read from.
        amount: Number of bytes to read.

    Returns:
        bytes: Bytes read from the socket.
    """

    return sock.recv(amount)


def receive_exactly(
    sock: socket.socket,
    amount: int,
    msg: str,
    expect: Sequence[int] | int,
) -> bytes:
    """Receive a specific amount of bytes and validate against expected values.

    Args:
        sock: Socket to read from.
        amount: Number of bytes to read.
        msg: Contextual message for error reporting.
        expect: Allowed response byte or sequence.

    Returns:
        bytes: Bytes read from the socket.
    """

    data = b""

    try:
        data += recv_decode(sock, 1)
    except OSError as err:
        raise OTAError(f"Error receiving acknowledge {msg}: {err}") from err

    try:
        check_error(data, expect)
    except OTAError as err:
        sock.close()
        raise OTAError(f"Error {msg}: {err}") from err

    while len(data) < amount:
        try:
            data += recv_decode(sock, amount - len(data))
        except OSError as err:
            raise OTAError(f"Error receiving {msg}: {err}") from err
    return data


def check_error(data: bytes, expect: Sequence[int] | int) -> None:
    """Raise OTAError if data does not match expected values.

    Args:
        data: First bytes received from the device.
        expect: Allowed response byte or sequence.
    """

    if not expect:
        return
    dat = data[0]
    if dat == RESPONSE_ERROR_MAGIC:
        raise OTAError("Error: Invalid magic byte")
    if dat == RESPONSE_ERROR_UPDATE_PREPARE:
        raise OTAError(
            "Error: Couldn't prepare flash memory for update. Is the binary too big? "
            "Please try restarting the ESP."
        )
    if dat == RESPONSE_ERROR_AUTH_INVALID:
        raise OTAError("Error: Authentication invalid. Is the password correct?")
    if dat == RESPONSE_ERROR_WRITING_FLASH:
        raise OTAError(
            "Error: Writing OTA data to flash memory failed. See USB logs for more "
            "information."
        )
    if dat == RESPONSE_ERROR_UPDATE_END:
        raise OTAError(
            "Error: Finishing update failed. See the MQTT/USB logs for more "
            "information."
        )
    if dat == RESPONSE_ERROR_INVALID_BOOTSTRAPPING:
        raise OTAError(
            "Error: Please press the reset button on the ESP. A manual reset is "
            "required on the first OTA-Update after flashing via USB."
        )
    if dat == RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG:
        raise OTAError(
            "Error: ESP has been flashed with wrong flash size. Please choose the "
            "correct 'board' option (esp01_1m always works) and then flash over USB."
        )
    if dat == RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG:
        raise OTAError(
            "Error: ESP does not have the requested flash size (wrong board). Please "
            "choose the correct 'board' option (esp01_1m always works) and try "
            "uploading again."
        )
    if dat == RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE:
        raise OTAError(
            "Error: ESP does not have enough space to store OTA file. Please try "
            "flashing a minimal firmware (remove everything except ota)"
        )
    if dat == RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE:
        raise OTAError(
            "Error: The OTA partition on the ESP is too small. ESPHome needs to resize "
            "this partition, please flash over USB."
        )
    if dat == RESPONSE_ERROR_NO_UPDATE_PARTITION:
        raise OTAError(
            "Error: The OTA partition on the ESP couldn't be found. ESPHome needs to create "
            "this partition, please flash over USB."
        )
    if dat == RESPONSE_ERROR_MD5_MISMATCH:
        raise OTAError(
            "Error: Application MD5 code mismatch. Please try again "
            "or flash over USB with a good quality cable."
        )
    if dat == RESPONSE_ERROR_UNKNOWN:
        raise OTAError("Unknown error from ESP")
    if not isinstance(expect, (list, tuple)):
        expect = [expect]
    if dat not in expect:
        raise OTAError(f"Unexpected response from ESP: 0x{data[0]:02X}")


def _get_stream_handler(logger: logging.Logger) -> logging.StreamHandler | None:
    """Return the first stream handler for a logger or its parents.

    Args:
        logger: Logger to inspect.

    Returns:
        logging.StreamHandler | None: First stream handler found, or ``None`` if none
            exist.
    """

    cur = logger
    while cur:
        for handler in cur.handlers:
            if isinstance(handler, logging.StreamHandler):
                return handler
        if not cur.propagate:
            break
        cur = cur.parent
    return None


def _write_stream(handler: logging.StreamHandler | None, text: str) -> None:
    """Write text to the handler or stdout if no handler exists.

    Args:
        handler: Stream handler to write to, or ``None`` for stdout.
        text: Text to output without automatically appending a newline.
    """

    if handler is None:
        print(text, end="", flush=True)
        return
    handler.acquire()
    try:
        handler.stream.write(text)
        handler.flush()
    finally:
        handler.release()


def log_info_inline(msg: str, logger: logging.Logger) -> None:
    """Emit an INFO-prefixed message without a trailing newline.

    Args:
        msg: Message to write.
        logger: Logger providing formatting and handler.
    """

    handler = _get_stream_handler(logger)
    if handler is None:
        _write_stream(None, f"INFO {msg}")
        return
    record = logger.makeRecord(logger.name, logging.INFO, "", 0, msg, (), None)
    formatted = handler.format(record)
    _write_stream(handler, formatted)


def log_inline_append(logger: logging.Logger, msg: str) -> None:
    """Append raw text to the current log line.

    Args:
        logger: Logger whose handler to write to.
        msg: Text to append.
    """

    handler = _get_stream_handler(logger)
    _write_stream(handler, msg)


def log_newline(logger: logging.Logger) -> None:
    """Terminate the current log line.

    Args:
        logger: Logger whose handler to write to.
    """

    handler = _get_stream_handler(logger)
    _write_stream(handler, "\n")


def _query_outq(sock: socket.socket, ioctl_cmd: int) -> int:
    """Return bytes currently queued for sending on the socket.

    Args:
        sock: Socket to query.
        ioctl_cmd: IOCTL command used to read the send queue.

    Returns:
        int: Number of bytes waiting in the kernel send queue.
    """

    import array
    import fcntl

    buf = array.array("I", [0])
    fcntl.ioctl(sock.fileno(), ioctl_cmd, buf, True)
    return int(buf[0])


def send_check(sock: socket.socket, data: bytes | list[int], log: str) -> None:
    """Send data over the socket and raise OTAError on failure.

    Args:
        sock: Socket to send over.
        data: Bytes to transmit.
        log: Description used in error messages.

    Raises:
        OTAError: If sending fails.
    """

    if isinstance(data, list):
        data = bytes(data)
    try:
        sock.sendall(data)
    except OSError as err:
        raise OTAError(f"Error sending {log}: {err}") from err


def _generate_auth_response(password: str, nonce: str, cnonce: str) -> str:
    """Return MD5 digest for password, nonce, and client nonce.

    Args:
        password: OTA password.
        nonce: Challenge string from the device.
        cnonce: Client-generated nonce.

    Returns:
        str: Hexadecimal MD5 digest string.
    """

    md5 = hashlib.md5()
    md5.update(password.encode("utf-8"))
    md5.update(nonce.encode())
    md5.update(cnonce.encode())
    return md5.hexdigest()


def _encode_upload_size(size: int) -> list[int]:
    """Encode an upload size as four big-endian bytes.

    Args:
        size: Upload size in bytes.

    Returns:
        list[int]: Four big-endian bytes.
    """

    return [
        (size >> 24) & 0xFF,
        (size >> 16) & 0xFF,
        (size >> 8) & 0xFF,
        size & 0xFF,
    ]


def perform_ota(
    remote_host: str, remote_port: int, password: str, filename: str
) -> None:
    """Send the OTA image to the remote host.

    Args:
        remote_host: Target host name or IP address.
        remote_port: OTA port on the device.
        password: OTA password for authentication.
        filename: Path to the firmware image file.

    Raises:
        OTAError: If the upload fails.
    """

    addresses = resolve_ip_address(remote_host, remote_port)
    sock: socket.socket | None = None
    remote_ip = None
    last_err: OSError | None = None
    for af, socktype, _proto, _canonname, sa in addresses:
        _LOGGER.info("Connecting to %s:%s...", sa[0], sa[1])
        s = socket.socket(af, socktype)
        s.settimeout(OTA_CONNECT_TIMEOUT)
        try:
            # Clamp TCP MSS so more packets fit into the device's small receive window.
            # Keeping more segments in flight makes fast retransmit more likely
            # and reduces the amount of RTO backoffs on loss.
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_MAXSEG, 536)
        except (AttributeError, OSError) as err:
            _LOGGER.warning("Could not set TCP MSS: %s", err)
        try:
            s.connect(sa)
        except OSError as err:
            last_err = err
            s.close()
            _LOGGER.error("Connecting to %s:%s failed: %s", sa[0], sa[1], err)
            continue
        sock = s
        remote_ip = sa[0]
        _LOGGER.info("Connected to %s", remote_ip)
        break
    if sock is None:
        raise OTAError(f"Error connecting to {remote_host}:{remote_port}: {last_err}")
    try:
        sock.settimeout(OTA_IMAGE_TRANSFER_TIMEOUT)

        with open(filename, "rb") as file:
            file_contents = file.read()
        file_size = len(file_contents)
        _LOGGER.info("Uploading %s (%s bytes)", filename, file_size)

        log_info_inline("Asking node for OTA version...", logger=_LOGGER)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        send_check(sock, MAGIC_BYTES, "magic bytes")
        _, version = receive_exactly(sock, 2, "version", RESPONSE_OK)
        log_inline_append(_LOGGER, " OK")
        log_newline(_LOGGER)

        _LOGGER.debug("Device support OTA version: %s", version)
        if version not in (OTA_VERSION_1_0, OTA_VERSION_2_0):
            raise OTAError(
                f"The node only supports OTA version {version}. Please use an OTA upgradable version."
            )

        log_info_inline("Asking node for features...", logger=_LOGGER)
        send_check(sock, [FEATURE_SUPPORTS_COMPRESSION], "features")
        features = receive_exactly(
            sock, 1, "features", [RESPONSE_HEADER_OK, RESPONSE_SUPPORTS_COMPRESSION]
        )[0]
        log_inline_append(_LOGGER, " OK")
        log_newline(_LOGGER)

        if features == RESPONSE_SUPPORTS_COMPRESSION:
            log_info_inline("Compressing image file...", logger=_LOGGER)
            upload_contents = gzip.compress(file_contents, compresslevel=9)
            log_inline_append(
                _LOGGER, f" OK: Compressed to {len(upload_contents)} bytes"
            )
            log_newline(_LOGGER)
        else:
            upload_contents = file_contents

        auth = receive_exactly(
            sock,
            1,
            "auth",
            [RESPONSE_REQUEST_AUTH, RESPONSE_AUTH_OK],
        )[0]
        if auth == RESPONSE_REQUEST_AUTH:
            if not password:
                raise OTAError("ESP requests password, but no password given!")
            nonce = receive_exactly(
                sock,
                32,
                "authentication nonce",
                [],
            ).decode()
            _LOGGER.debug("Auth: Nonce is %s", nonce)
            cnonce = hashlib.md5(str(random.random()).encode()).hexdigest()
            _LOGGER.debug("Auth: CNonce is %s", cnonce)
            log_info_inline(
                "Authorize with node to do the update...",
                logger=_LOGGER,
            )
            send_check(sock, cnonce.encode(), "auth cnonce")
            result = _generate_auth_response(password, nonce, cnonce)
            send_check(sock, result.encode(), "auth result")
            receive_exactly(sock, 1, "auth result", RESPONSE_AUTH_OK)
            log_inline_append(_LOGGER, " OK")
            log_newline(_LOGGER)

        upload_size = len(upload_contents)
        log_info_inline(
            "Asking node if firmware image size can be accepted...",
            logger=_LOGGER,
        )
        send_check(sock, _encode_upload_size(upload_size), "binary size")
        receive_exactly(sock, 1, "binary size", RESPONSE_UPDATE_PREPARE_OK)
        log_inline_append(_LOGGER, " OK")
        log_newline(_LOGGER)

        upload_md5 = hashlib.md5(upload_contents).hexdigest()
        _LOGGER.debug("MD5 of upload is %s", upload_md5)
        log_info_inline(
            "Providing node with md5 checksum for firmware...", logger=_LOGGER
        )
        send_check(sock, upload_md5.encode(), "file checksum")
        receive_exactly(sock, 1, "file checksum", RESPONSE_BIN_MD5_OK)
        log_inline_append(_LOGGER, " OK")
        log_newline(_LOGGER)

        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        _LOGGER.info("Starting to send firmware image to node...")
        progress = ProgressBar()
        start_time = time.perf_counter()
        try:
            if version == OTA_VERSION_2_0:
                image_transfer_with_device_acks_nonblocking(
                    sock,
                    upload_contents,
                    progress=progress,
                )
            else:
                sock.sendall(upload_contents)
        except OSError as err:
            sys.stderr.write("\n\n")
            raise OTAError(f"Error sending data: {err}") from err
        except OTAError:
            sys.stderr.write("\n\n")
            raise
        if version < OTA_VERSION_2_0:
            progress.update(1.0)
        progress.done()

        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        receive_exactly(sock, 1, "receive OK", RESPONSE_RECEIVE_OK)
        receive_exactly(sock, 1, "update end", RESPONSE_UPDATE_END_OK)
        send_check(sock, [RESPONSE_OK], "end acknowledgement")
        duration = time.perf_counter() - start_time
        _LOGGER.info("Upload complete in %.2f seconds", duration)
        _LOGGER.info("OTA successful")
        time.sleep(1)

    finally:
        sock.close()


def run_ota_impl_(
    remote_host: str, remote_port: int, password: str, filename: str
) -> int:
    """Run OTA update implementation.

    Args:
        remote_host: Target host name or IP address.
        remote_port: OTA port on the device.
        password: OTA password for authentication.
        filename: Path to the firmware image file.

    Returns:
        int: ``0`` on success, ``1`` on failure.
    """

    try:
        perform_ota(remote_host, remote_port, password, filename)
    except OTAError as err:
        _LOGGER.error(err)
        return 1
    return 0


def run_ota(remote_host: str, remote_port: int, password: str, filename: str) -> int:
    """Convenience wrapper around ``run_ota_impl_``.

    Args:
        remote_host: Target host name or IP address.
        remote_port: OTA port on the device.
        password: OTA password for authentication.
        filename: Path to the firmware image file.

    Returns:
        int: ``0`` on success, ``1`` on failure.
    """

    return run_ota_impl_(remote_host, remote_port, password, filename)


def image_transfer_with_device_acks_nonblocking(
    sock: socket.socket,
    data: bytes,
    *,
    progress: ProgressBar | None = None,
) -> None:
    """Transfer firmware while processing device ACKs.

    On Linux systems, attempt to track TCP ACKs via ``SIOCOUTQ`` for smoother
    progress reporting. If unavailable, fall back to device ACK bytes.

    Args:
        sock: Connected socket to the device.
        data: Firmware image bytes to send.
        progress: Optional progress bar for updates.

    Raises:
        OTAError: If the device closes early or protocol errors occur.
    """

    ack_block_size = OTA_IMAGE_ACK_SIZE_OTA_V2
    stall_timeout = float(OTA_IMAGE_TRANSFER_TIMEOUT)

    _ack = RESPONSE_CHUNK_OK
    if isinstance(_ack, int):
        if not 0 <= _ack <= 255:
            raise ValueError("RESPONSE_CHUNK_OK int must be in range 0..255")
        ack_byte = bytes([_ack])
    elif isinstance(_ack, (bytes, bytearray)) and len(_ack) == 1:
        ack_byte = bytes(_ack)
    else:
        raise ValueError(
            "RESPONSE_CHUNK_OK must be an int or a single-byte bytes object"
        )

    view = memoryview(data)
    total = len(view)
    prev_blocking = sock.getblocking()
    sock.setblocking(False)

    sel = selectors.DefaultSelector()
    sel.register(sock, selectors.EVENT_READ | selectors.EVENT_WRITE)

    use_sock_ack = False
    outq_baseline = 0
    ioctl_cmd = 0
    if sys.platform.startswith("linux"):
        ioctl_cmd = getattr(socket, "SIOCOUTQ", 0x5411)
        try:
            outq_baseline = _query_outq(sock, ioctl_cmd)
        except OSError:
            pass
        else:
            use_sock_ack = True

    try:
        _transfer_with_acks(
            sel,
            sock,
            view,
            total,
            ack_block_size,
            stall_timeout,
            ack_byte[0],
            progress,
            use_sock_ack,
            outq_baseline,
            ioctl_cmd,
        )
    finally:
        with suppress(KeyError):
            sel.unregister(sock)
        sock.setblocking(prev_blocking)


def _transfer_with_acks(
    sel: selectors.BaseSelector,
    sock: socket.socket,
    view: memoryview,
    total: int,
    ack_block_size: int,
    stall_timeout: float,
    ack_val: int,
    progress: ProgressBar | None,
    use_sock_ack: bool,
    outq_baseline: int,
    ioctl_cmd: int,
) -> None:
    """Send data while tracking progress via device ACKs or the TCP send queue.

    Args:
        sel: Selector monitoring socket readiness.
        sock: Connected socket to the device.
        view: Firmware image as a memoryview.
        total: Total number of bytes to send.
        ack_block_size: Bytes per device ACK segment.
        stall_timeout: Seconds without activity before considering the transfer
            stalled.
        ack_val: Byte value used by the device for ACKs.
        progress: Optional progress bar to update.
        use_sock_ack: Whether TCP send queue tracking is enabled.
        outq_baseline: Initial kernel send queue size for outstanding
            calculation.
        ioctl_cmd: ``SIOCOUTQ`` constant for querying the send queue.

    Raises:
        OTAError: If the transfer stalls or the device sends unexpected data.
    """

    sent = 0
    acked_device = 0
    acked_segments = 0
    acked_tcp = 0
    last_activity = time.perf_counter()
    last_update = last_activity
    next_force_at = last_update + PROGRESS_TIMER_FORCE_SEC
    total_segments = (total + ack_block_size - 1) // ack_block_size
    acked_fallback = 0

    def _render_progress() -> bool:
        nonlocal acked_tcp, use_sock_ack, acked_fallback
        if progress is None:
            return False
        if use_sock_ack:
            try:
                outq_now = _query_outq(sock, ioctl_cmd)
            except OSError:
                use_sock_ack = False
                acked_fallback = max(acked_fallback, acked_tcp)
                ratio = max(acked_device, acked_fallback) / total
                percent_int = int(round(min(max(0.0, ratio), 1.0) * 10000))
                seg_pair = (None, None)
                if (
                    percent_int == progress.get_last_percent_int()
                    and seg_pair == progress.get_last_segments()
                ):
                    return False
                with suppress(AttributeError, ValueError):
                    progress.update(ratio)
                return True
            ours_outstanding = max(0, outq_now - outq_baseline)
            acked_tcp = max(0, min(sent, sent - ours_outstanding))
            ratio = acked_tcp / total
            percent_int = int(round(min(max(0.0, ratio), 1.0) * 10000))
            seg_pair = (acked_segments, total_segments)
            if (
                percent_int == progress.get_last_percent_int()
                and seg_pair == progress.get_last_segments()
            ):
                return False
            with suppress(AttributeError, ValueError):
                progress.update(
                    ratio,
                    acked_segments=acked_segments,
                    total_segments=total_segments,
                )
            return True
        ratio = max(acked_device, acked_fallback) / total
        percent_int = int(round(min(max(0.0, ratio), 1.0) * 10000))
        seg_pair = (None, None)
        if (
            percent_int == progress.get_last_percent_int()
            and seg_pair == progress.get_last_segments()
        ):
            return False
        with suppress(AttributeError, ValueError):
            progress.update(ratio)
        return True

    while acked_device < total:
        now = time.perf_counter()
        timeout = min(
            stall_timeout,
            PROGRESS_UPDATE_THROTTLE_SEC,
            max(0.0, next_force_at - now),
        )
        events = sel.select(timeout=timeout)
        redraw = False

        for _, mask in events:
            if mask & selectors.EVENT_WRITE:
                if sent < total:
                    try:
                        n = sock.send(view[sent:])
                    except (BlockingIOError, InterruptedError):
                        n = 0
                    if n > 0:
                        sent += n
                        last_activity = time.perf_counter()
                else:
                    n = 0
                now = time.perf_counter()
                if (
                    progress is not None
                    and now - last_update >= PROGRESS_UPDATE_THROTTLE_SEC
                    and _render_progress()
                ):
                    last_update = time.perf_counter()
                    next_force_at = last_update + PROGRESS_TIMER_FORCE_SEC
                    redraw = True

            if mask & selectors.EVENT_READ:
                try:
                    peek = sock.recv(4096, socket.MSG_PEEK)
                except (BlockingIOError, InterruptedError):
                    peek = b""

                if peek == b"":
                    if acked_device < total:
                        raise OTAError(
                            f"Peer closed early (acked {acked_device} / {total})"
                        )
                    break

                i = 0
                for b in peek:
                    if b == ack_val:
                        i += 1
                    else:
                        break

                if i > 0:
                    sock.recv(i)
                    acked_device = min(acked_device + i * ack_block_size, total)
                    acked_segments = min(acked_segments + i, total_segments)
                    last_activity = time.perf_counter()
                    if progress is not None and _render_progress():
                        last_update = time.perf_counter()
                        next_force_at = last_update + PROGRESS_TIMER_FORCE_SEC
                        redraw = True
                elif acked_device < total:
                    first = peek[0]
                    raise OTAError(
                        f"Unexpected byte 0x{first:02X} from device before upload completed"
                    )

        now = time.perf_counter()
        if (
            progress is not None
            and not redraw
            and use_sock_ack
            and now >= next_force_at
        ):
            if _render_progress():
                last_update = time.perf_counter()
                next_force_at = last_update + PROGRESS_TIMER_FORCE_SEC
                redraw = True
            else:
                next_force_at = now + PROGRESS_TIMER_FORCE_SEC

        if time.perf_counter() - last_activity > stall_timeout:
            raise OTAError(
                f"Stall: no send progress/protocol ack received within {stall_timeout} seconds"
            )
