from __future__ import annotations

from collections.abc import Callable
import gzip
import hashlib
import io
import logging
from pathlib import Path
import secrets
import socket
import sys
import time
from typing import Any

from esphome.core import EsphomeError
from esphome.helpers import ProgressBar, resolve_ip_address

OTA_TYPE_UPDATE_APP = 0x00
OTA_TYPE_UPDATE_PARTITION_TABLE = 0x01
OTA_TYPE_UPDATE_BOOTLOADER = 0x02

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
RESPONSE_FEATURE_FLAGS = 0x48

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
RESPONSE_ERROR_RP2040_NOT_ENOUGH_SPACE = 0x8C
RESPONSE_ERROR_SIGNATURE_INVALID = 0x8D
RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE = 0x8E
RESPONSE_ERROR_PARTITION_TABLE_VERIFY = 0x8F
RESPONSE_ERROR_PARTITION_TABLE_UPDATE = 0x90
RESPONSE_ERROR_BOOTLOADER_VERIFY = 0x91
RESPONSE_ERROR_BOOTLOADER_UPDATE = 0x92
RESPONSE_ERROR_UNKNOWN = 0xFF

OTA_VERSION_1_0 = 1
OTA_VERSION_2_0 = 2

MAGIC_BYTES = [0x6C, 0x26, 0xF7, 0x5C, 0x45]

CLIENT_FEATURE_SUPPORTS_COMPRESSION = 0x01
CLIENT_FEATURE_SUPPORTS_SHA256_AUTH = 0x02
CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL = 0x04
SERVER_FEATURE_SUPPORTS_COMPRESSION = 0x01
SERVER_FEATURE_SUPPORTS_PARTITION_ACCESS = 0x02

# OTA types this client knows how to send. Future PRs that add bootloader/partition
# updates extend this set. Anything outside the set is rejected up front so callers
# of perform_ota/run_ota get a clear error instead of a post-auth 0x8E from the device.
_SUPPORTED_OTA_TYPES: frozenset[int] = frozenset(
    {OTA_TYPE_UPDATE_APP, OTA_TYPE_UPDATE_PARTITION_TABLE, OTA_TYPE_UPDATE_BOOTLOADER}
)

UPLOAD_BLOCK_SIZE = 8192
UPLOAD_BUFFER_SIZE = UPLOAD_BLOCK_SIZE * 8

_LOGGER = logging.getLogger(__name__)

# Authentication method lookup table: response -> (hash_func, nonce_size, name)
_AUTH_METHODS: dict[int, tuple[Callable[..., Any], int, str]] = {
    RESPONSE_REQUEST_SHA256_AUTH: (hashlib.sha256, 64, "SHA256"),
    RESPONSE_REQUEST_AUTH: (hashlib.md5, 32, "MD5"),
}

# Error response code -> human-readable message (without the "Error: " prefix; check_error()
# prepends it uniformly). Looked up by check_error() to translate a single byte from the device
# into an OTAError. Add new error codes here rather than extending the if-chain in check_error().
_ERROR_MESSAGES: dict[int, str] = {
    RESPONSE_ERROR_MAGIC: "Invalid magic byte",
    RESPONSE_ERROR_UPDATE_PREPARE: (
        "Couldn't prepare flash memory for update. Is the binary too big? "
        "Please try restarting the ESP."
    ),
    RESPONSE_ERROR_AUTH_INVALID: "Authentication invalid. Is the password correct?",
    RESPONSE_ERROR_WRITING_FLASH: (
        "Writing OTA data to flash memory failed. See USB logs for more information."
    ),
    RESPONSE_ERROR_UPDATE_END: (
        "Finishing update failed. See the MQTT/USB logs for more information."
    ),
    RESPONSE_ERROR_INVALID_BOOTSTRAPPING: (
        "Please press the reset button on the ESP. A manual reset is "
        "required on the first OTA-Update after flashing via USB."
    ),
    RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG: (
        "ESP has been flashed with wrong flash size. Please choose the "
        "correct 'board' option (esp01_1m always works) and then flash over USB."
    ),
    RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG: (
        "ESP does not have the requested flash size (wrong board). Please "
        "choose the correct 'board' option (esp01_1m always works) and try "
        "uploading again."
    ),
    RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE: (
        "ESP does not have enough space to store OTA file. Please try "
        "flashing a minimal firmware (remove everything except ota)"
    ),
    RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE: (
        "The OTA partition on the ESP is too small. ESPHome needs to resize "
        "this partition, please flash over USB."
    ),
    RESPONSE_ERROR_NO_UPDATE_PARTITION: (
        "The OTA partition on the ESP couldn't be found. ESPHome needs to "
        "create this partition, please flash over USB."
    ),
    RESPONSE_ERROR_MD5_MISMATCH: (
        "Application MD5 code mismatch. Please try again "
        "or flash over USB with a good quality cable."
    ),
    RESPONSE_ERROR_SIGNATURE_INVALID: (
        "Firmware signature verification failed. The firmware was not signed "
        "with the correct key. Ensure the signing key matches the one used to build "
        "the firmware currently running on the device."
    ),
    RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE: (
        "The requested OTA type is not supported by the device."
    ),
    RESPONSE_ERROR_PARTITION_TABLE_VERIFY: (
        "The partition table update could not be verified. No changes were "
        "made to the flash content. Check the logs for more information and retry."
    ),
    RESPONSE_ERROR_PARTITION_TABLE_UPDATE: (
        "An error occurred while updating the partition table. The device is now "
        "in a degraded state and may not be able to boot. Open the logs and retry "
        "the partition table update without rebooting the device. If the device "
        "fails to boot, recover it via a serial flash."
    ),
    RESPONSE_ERROR_BOOTLOADER_VERIFY: (
        "The bootloader update could not be verified. No changes were "
        "made to the bootloader. Check the logs for more information and retry."
    ),
    RESPONSE_ERROR_BOOTLOADER_UPDATE: (
        "An error occurred while updating the bootloader. The device is now "
        "in a degraded state and may not be able to boot. Open the logs and retry "
        "the bootloader update without rebooting the device. If the device "
        "fails to boot, recover it via a serial flash."
    ),
    RESPONSE_ERROR_UNKNOWN: "Unknown error from ESP",
}


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
    # Detect device errors and connection-closed cases regardless of `expect`. If we
    # only ran these checks when expect was set, error bytes returned during
    # accept-any-response reads (e.g. feature negotiation, auth nonces) would be
    # silently passed through and surface later as cryptic decode/timeout failures.
    if not data:
        raise OTAError(
            "Error: Device closed connection without responding. "
            "This may indicate the device ran out of memory, "
            "a network issue, or the connection was interrupted."
        )
    dat = data[0]
    error_msg = _ERROR_MESSAGES.get(dat)
    if error_msg is not None:
        raise OTAError(f"Error: {error_msg}")
    if expect is None:
        return
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
    sock: socket.socket,
    password: str | None,
    file_handle: io.IOBase,
    filename: Path,
    ota_type: int = OTA_TYPE_UPDATE_APP,
) -> None:
    # Validate ota_type up front. It travels as a single byte on the wire, and
    # passing an out-of-range value would only surface as a ValueError from
    # bytes([ota_type]) deep inside send_check, bypassing OTAError handling.
    if not isinstance(ota_type, int) or not 0 <= ota_type <= 0xFF:
        raise OTAError(
            f"Invalid ota_type {ota_type!r}; expected an integer in range 0-255"
        )
    if ota_type not in _SUPPORTED_OTA_TYPES:
        supported = ", ".join(f"0x{t:02X}" for t in sorted(_SUPPORTED_OTA_TYPES))
        raise OTAError(
            f"Unsupported OTA type 0x{ota_type:02X}; this ESPHome supports: {supported}"
        )

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
    features_to_send = (
        CLIENT_FEATURE_SUPPORTS_COMPRESSION
        | CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
        | CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
    )
    send_check(sock, features_to_send, "features")
    features = receive_exactly(
        sock,
        1,
        "features",
        None,  # Accept any response
    )[0]

    extended_proto = False
    if features == RESPONSE_FEATURE_FLAGS:
        extended_proto = True
        features = receive_exactly(
            sock,
            1,
            "feature flags",
            None,  # Accept any response
        )[0]
    elif features == RESPONSE_SUPPORTS_COMPRESSION:
        features = SERVER_FEATURE_SUPPORTS_COMPRESSION
    else:
        features = 0

    if ota_type != OTA_TYPE_UPDATE_APP:
        # Any non-app OTA type requires the extended protocol and the
        # partition-access server feature. Reject up front so the user gets
        # a clear capability error instead of a post-auth 0x8E from the device.
        flag_name = {
            OTA_TYPE_UPDATE_PARTITION_TABLE: "--partition-table",
            OTA_TYPE_UPDATE_BOOTLOADER: "--bootloader",
        }.get(ota_type, f"OTA type 0x{ota_type:02X}")
        if not extended_proto:
            raise OTAError(
                f"Device does not support the extended OTA protocol that "
                f"{flag_name} requires. The running firmware is too old; "
                f"recompile and upload a current ESPHome firmware via a "
                f"regular OTA (without {flag_name}), then retry."
            )
        if not (features & SERVER_FEATURE_SUPPORTS_PARTITION_ACCESS):
            raise OTAError(
                f"The running firmware was built without "
                f"'allow_partition_access: true', so {flag_name} cannot be "
                f"used. Add the option to the esphome OTA platform in your "
                f"YAML, recompile and upload (without {flag_name}), then "
                f"retry {flag_name}."
            )

    if features & SERVER_FEATURE_SUPPORTS_COMPRESSION:
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
            sock, nonce_size, f"{hash_name} authentication nonce", None, decode=False
        )
        assert isinstance(nonce_bytes, bytes)
        nonce = nonce_bytes.decode()
        _LOGGER.debug("Auth: %s Nonce is %s", hash_name, nonce)

        # Generate cnonce matching the hash algorithm's digest size
        cnonce = secrets.token_hex(nonce_size // 2)
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

    if extended_proto:
        send_check(sock, ota_type, "ota type")

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
    progress = ProgressBar("Uploading")
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
    remote_host: str | list[str],
    remote_port: int,
    password: str | None,
    filename: Path,
    ota_type: int = OTA_TYPE_UPDATE_APP,
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
        if not CORE.dashboard:
            _LOGGER.error("(If you know the IP, try --device <IP>)")
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
                perform_ota(sock, password, file_handle, filename, ota_type)
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
    remote_host: str | list[str],
    remote_port: int,
    password: str | None,
    filename: Path,
    ota_type: int = OTA_TYPE_UPDATE_APP,
) -> tuple[int, str | None]:
    try:
        return run_ota_impl_(remote_host, remote_port, password, filename, ota_type)
    except OTAError as err:
        _LOGGER.error(err)
        return 1, None
