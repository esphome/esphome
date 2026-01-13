from __future__ import annotations

from collections.abc import Callable
import gzip
import hashlib
import io
import logging
from pathlib import Path
import random
import socket
import sys
import time
from typing import Any

from esphome.core import EsphomeError
from esphome.helpers import resolve_ip_address

RESPONSE_OK = 0x00
RESPONSE_REQUEST_AUTH = 0x01
RESPONSE_REQUEST_SHA256_AUTH = 0x02

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
FEATURE_SUPPORTS_SHA256_AUTH = 0x02


UPLOAD_BLOCK_SIZE = 8192
UPLOAD_BUFFER_SIZE = UPLOAD_BLOCK_SIZE * 8

_LOGGER = logging.getLogger(__name__)

# Authentication method lookup table: response -> (hash_func, nonce_size, name)
_AUTH_METHODS: dict[int, tuple[Callable[..., Any], int, str]] = {
    RESPONSE_REQUEST_SHA256_AUTH: (hashlib.sha256, 64, "SHA256"),
    RESPONSE_REQUEST_AUTH: (hashlib.md5, 32, "MD5"),
}


class ProgressBar:
    def __init__(self):
        self.last_progress = None

    def update(self, progress):
        bar_length = 60
        status = ""
        if progress >= 1:
            progress = 1
            status = "Done...\r\n"
        new_progress = int(progress * 100)
        if new_progress == self.last_progress:
            return
        self.last_progress = new_progress
        block = int(round(bar_length * progress))
        text = f"\rUploading: [{'=' * block + ' ' * (bar_length - block)}] {new_progress}% {status}"
        sys.stderr.write(text)
        sys.stderr.flush()

    def done(self):
        sys.stderr.write("\n")
        sys.stderr.flush()


class OTAError(EsphomeError):
    pass


def recv_decode(
    sock: socket.socket, amount: int, decode: bool = True
) -> bytes | list[int]:
    """Receive data from socket and optionally decode to list of integers.

    :param sock: Socket to receive data from.
    :param amount: Number of bytes to receive.
    :param decode: If True, convert bytes to list of integers, otherwise return raw bytes.
    :return: List of integers if decode=True, otherwise raw bytes.
    """
    data = sock.recv(amount)
    if not decode:
        return data
    return list(data)


def receive_exactly(
    sock: socket.socket,
    amount: int,
    msg: str,
    expect: int | list[int] | None,
    decode: bool = True,
) -> list[int] | bytes:
    """Receive exactly the specified amount of data from socket with error checking.

    :param sock: Socket to receive data from.
    :param amount: Exact number of bytes to receive.
    :param msg: Description of what is being received for error messages.
    :param expect: Expected response code(s) for validation, None to skip validation.
    :param decode: If True, return list of integers, otherwise return raw bytes.
    :return: List of integers if decode=True, otherwise raw bytes.
    :raises OTAError: If receiving fails or response doesn't match expected.
    """
    data: list[int] | bytes = [] if decode else b""

    try:
        data += recv_decode(sock, 1, decode=decode)  # type: ignore[operator]
    except OSError as err:
        raise OTAError(f"Error receiving acknowledge {msg}: {err}") from err

    try:
        check_error(data, expect)
    except OTAError as err:
        sock.close()
        raise OTAError(f"Error {msg}: {err}") from err

    while len(data) < amount:
        try:
            data += recv_decode(sock, amount - len(data), decode=decode)  # type: ignore[operator]
        except OSError as err:
            raise OTAError(f"Error receiving {msg}: {err}") from err
    return data


def check_error(data: list[int] | bytes, expect: int | list[int] | None) -> None:
    """Check response data for error codes and validate against expected response.

    :param data: Response data from device (first byte is the response code).
    :param expect: Expected response code(s), None to skip validation.
    :raises OTAError: If an error code is detected or response doesn't match expected.
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


def send_check(
    sock: socket.socket, data: list[int] | tuple[int, ...] | int | str | bytes, msg: str
) -> None:
    """Send data to socket with error handling.

    :param sock: Socket to send data to.
    :param data: Data to send (can be list/tuple of ints, single int, string, or bytes).
    :param msg: Description of what is being sent for error messages.
    :raises OTAError: If sending fails.
    """
    try:
        if isinstance(data, (list, tuple)):
            data = bytes(data)
        elif isinstance(data, int):
            data = bytes([data])
        elif isinstance(data, str):
            data = data.encode("utf8")

        sock.sendall(data)
    except OSError as err:
        raise OTAError(f"Error sending {msg}: {err}") from err


def perform_ota(
    sock: socket.socket, password: str | None, file_handle: io.IOBase, filename: Path
) -> None:
    file_contents = file_handle.read()
    file_size = len(file_contents)
    _LOGGER.info("Uploading %s (%s bytes)", filename, file_size)

    # Enable nodelay, we need it for phase 1
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    send_check(sock, MAGIC_BYTES, "magic bytes")

    _, version = receive_exactly(sock, 2, "version", RESPONSE_OK)
    _LOGGER.debug("Device support OTA version: %s", version)
    supported_versions = (OTA_VERSION_1_0, OTA_VERSION_2_0)
    if version not in supported_versions:
        raise OTAError(
            f"Device uses unsupported OTA version {version}, this ESPHome supports {supported_versions}"
        )

    # Features - send both compression and SHA256 auth support
    features_to_send = FEATURE_SUPPORTS_COMPRESSION | FEATURE_SUPPORTS_SHA256_AUTH
    send_check(sock, features_to_send, "features")
    features = receive_exactly(
        sock,
        1,
        "features",
        None,  # Accept any response
    )[0]

    if features == RESPONSE_SUPPORTS_COMPRESSION:
        upload_contents = gzip.compress(file_contents, compresslevel=9)
        _LOGGER.info("Compressed to %s bytes", len(upload_contents))
    else:
        upload_contents = file_contents

    def perform_auth(
        sock: socket.socket,
        password: str | None,
        hash_func: Callable[..., Any],
        nonce_size: int,
        hash_name: str,
    ) -> None:
        """Perform challenge-response authentication using specified hash algorithm."""
        if password is None:
            raise OTAError("ESP requests password, but no password given!")

        nonce_bytes = receive_exactly(
            sock, nonce_size, f"{hash_name} authentication nonce", [], decode=False
        )
        assert isinstance(nonce_bytes, bytes)
        nonce = nonce_bytes.decode()
        _LOGGER.debug("Auth: %s Nonce is %s", hash_name, nonce)

        # Generate cnonce
        cnonce = hash_func(str(random.random()).encode()).hexdigest()
        _LOGGER.debug("Auth: %s CNonce is %s", hash_name, cnonce)

        send_check(sock, cnonce, "auth cnonce")

        # Calculate challenge response
        hasher = hash_func()
        hasher.update(password.encode("utf-8"))
        hasher.update(nonce.encode())
        hasher.update(cnonce.encode())
        result = hasher.hexdigest()
        _LOGGER.debug("Auth: %s Result is %s", hash_name, result)

        send_check(sock, result, "auth result")
        receive_exactly(sock, 1, "auth result", RESPONSE_AUTH_OK)

    (auth,) = receive_exactly(
        sock,
        1,
        "auth",
        [RESPONSE_REQUEST_AUTH, RESPONSE_REQUEST_SHA256_AUTH, RESPONSE_AUTH_OK],
    )

    if auth != RESPONSE_AUTH_OK:
        hash_func, nonce_size, hash_name = _AUTH_METHODS[auth]
        perform_auth(sock, password, hash_func, nonce_size, hash_name)

    # Timeout must match device-side OTA_SOCKET_TIMEOUT_DATA to prevent premature failures
    sock.settimeout(90.0)

    upload_size = len(upload_contents)
    upload_size_encoded = [
        (upload_size >> 24) & 0xFF,
        (upload_size >> 16) & 0xFF,
        (upload_size >> 8) & 0xFF,
        (upload_size >> 0) & 0xFF,
    ]
    send_check(sock, upload_size_encoded, "binary size")
    receive_exactly(sock, 1, "binary size", RESPONSE_UPDATE_PREPARE_OK)

    upload_md5 = hashlib.md5(upload_contents).hexdigest()
    _LOGGER.debug("MD5 of upload is %s", upload_md5)

    send_check(sock, upload_md5, "file checksum")
    receive_exactly(sock, 1, "file checksum", RESPONSE_BIN_MD5_OK)

    # Disable nodelay for transfer
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 0)
    # Limit send buffer (usually around 100kB) in order to have progress bar
    # show the actual progress

    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, UPLOAD_BUFFER_SIZE)
    start_time = time.perf_counter()

    offset = 0
    progress = ProgressBar()
    while True:
        chunk = upload_contents[offset : offset + UPLOAD_BLOCK_SIZE]
        if not chunk:
            break
        offset += len(chunk)

        try:
            sock.sendall(chunk)
            if version >= OTA_VERSION_2_0:
                receive_exactly(sock, 1, "chunk OK", RESPONSE_CHUNK_OK)
        except OSError as err:
            sys.stderr.write("\n")
            raise OTAError(f"Error sending data: {err}") from err

        progress.update(offset / upload_size)
    progress.done()

    # Enable nodelay for last checks
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    duration = time.perf_counter() - start_time

    _LOGGER.info("Upload took %.2f seconds, waiting for result...", duration)

    receive_exactly(sock, 1, "receive OK", RESPONSE_RECEIVE_OK)
    receive_exactly(sock, 1, "Update end", RESPONSE_UPDATE_END_OK)
    send_check(sock, RESPONSE_OK, "end acknowledgement")

    _LOGGER.info("OTA successful")

    # Do not connect logs until it is fully on
    time.sleep(1)


def run_ota_impl_(
    remote_host: str | list[str], remote_port: int, password: str | None, filename: Path
) -> tuple[int, str | None]:
    from esphome.core import CORE

    # Handle both single host and list of hosts
    try:
        # Resolve all hosts at once for parallel DNS resolution
        res = resolve_ip_address(
            remote_host, remote_port, address_cache=CORE.address_cache
        )
    except EsphomeError as err:
        _LOGGER.error(
            "Error resolving IP address of %s. Is it connected to WiFi?",
            remote_host,
        )
        _LOGGER.error(
            "(If this error persists, please set a static IP address: "
            "https://esphome.io/components/wifi/#manual-ips)"
        )
        raise OTAError(err) from err

    for r in res:
        af, socktype, _, _, sa = r
        _LOGGER.info("Connecting to %s port %s...", sa[0], sa[1])
        sock = socket.socket(af, socktype)
        sock.settimeout(20.0)
        try:
            sock.connect(sa)
        except OSError as err:
            sock.close()
            _LOGGER.error("Connecting to %s port %s failed: %s", sa[0], sa[1], err)
            continue

        _LOGGER.info("Connected to %s", sa[0])
        with open(filename, "rb") as file_handle:
            try:
                perform_ota(sock, password, file_handle, filename)
            except OTAError as err:
                _LOGGER.error(str(err))
                return 1, None
            finally:
                sock.close()

        # Successfully uploaded to sa[0]
        return 0, sa[0]

    _LOGGER.error("Connection failed.")
    return 1, None


def run_ota(
    remote_host: str | list[str], remote_port: int, password: str | None, filename: Path
) -> tuple[int, str | None]:
    try:
        return run_ota_impl_(remote_host, remote_port, password, filename)
    except OTAError as err:
        _LOGGER.error(err)
        return 1, None
