from __future__ import annotations

from collections.abc import Callable
import contextlib
import gzip
import hashlib
import io
import logging
from pathlib import Path
import secrets
import socket
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
RESPONSE_ERROR_VERSION_DOWNGRADE = 0x93
RESPONSE_ERROR_ENCRYPTION_REQUIRED = 0x94
RESPONSE_ERROR_UNKNOWN = 0xFF

OTA_VERSION_1_0 = 1
OTA_VERSION_2_0 = 2

MAGIC_BYTES = [0x6C, 0x26, 0xF7, 0x5C, 0x45]

CLIENT_FEATURE_SUPPORTS_COMPRESSION = 0x01
CLIENT_FEATURE_SUPPORTS_SHA256_AUTH = 0x02
CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL = 0x04
CLIENT_FEATURE_SUPPORTS_NOISE = 0x08
SERVER_FEATURE_SUPPORTS_COMPRESSION = 0x01
SERVER_FEATURE_SUPPORTS_PARTITION_ACCESS = 0x02
SERVER_FEATURE_SUPPORTS_NOISE = 0x04

NOISE_FRAME_INDICATOR = 0x01
NOISE_HANDSHAKE_OK = 0x00
# The device decrypts frames in its transfer buffer (OTA_BUFFER_SIZE, sized
# as this plus the 16-byte ChaCha20-Poly1305 MAC). 1024 divides the 8192-byte
# upload block exactly, so blocks tile into full frames with no runt.
NOISE_MAX_PLAINTEXT = 1024
# Wire contract: the device sends exactly this reject reason for a bad MAC
NOISE_MAC_FAILURE_REASON = "Handshake MAC failure"
NOISE_PROLOGUE_INIT = b"NoiseOTAInit"

# OTA types this client knows how to send. Future PRs that add bootloader/partition
# updates extend this set. Anything outside the set is rejected up front so callers
# of perform_ota/run_ota get a clear error instead of a post-auth 0x8E from the device.
_SUPPORTED_OTA_TYPES: frozenset[int] = frozenset(
    {OTA_TYPE_UPDATE_APP, OTA_TYPE_UPDATE_PARTITION_TABLE, OTA_TYPE_UPDATE_BOOTLOADER}
)

UPLOAD_BLOCK_SIZE = 8192
UPLOAD_BUFFER_SIZE = UPLOAD_BLOCK_SIZE * 8

# Flaky Wi-Fi links often drop the first OTA attempt, and the device may need time
# to clean up a half-open connection (its handshake watchdog runs at 20s) before it
# accepts a new one, so wait between attempts instead of failing the upload outright.
# Every resolved address is tried once, and this many extra attempts are shared
# across the addresses on top of that.
EXTRA_UPLOAD_ATTEMPTS = 2
UPLOAD_RETRY_DELAY = 5.0

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
        "this partition. Please flash over USB or update the partition table "
        "over the air."
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
    RESPONSE_ERROR_VERSION_DOWNGRADE: (
        "The device rejected the update because it has OTA downgrade protection "
        "enabled: the new firmware's version must be newer than the version the "
        "device is currently running."
    ),
    RESPONSE_ERROR_ENCRYPTION_REQUIRED: (
        "The device requires an encrypted OTA connection but this upload has no "
        "encryption key. Add 'encryption:' to the 'ota: platform: esphome' section "
        "of the YAML this upload uses, or update your esphome installation if it "
        "predates OTA encryption."
    ),
    RESPONSE_ERROR_UNKNOWN: "Unknown error from ESP",
}


class OTAError(EsphomeError):
    pass


class OTANetworkError(OTAError):
    """Network-level OTA failure (timeout, reset, closed connection); retrying may succeed."""


def _committed_error(err: OTANetworkError) -> OTAError:
    """Wrap a network failure that happened once the device had the full image.

    Past that point the device commits and reboots on its own, so the failure
    must not be retried; a re-upload could flash a device that already updated.
    """
    return OTAError(
        f"{err} (the device may have already committed the update and "
        f"be rebooting; check whether it comes back with the new "
        f"firmware before uploading again)"
    )


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
        raise OTANetworkError(f"receiving {msg} response: {err}") from err

    try:
        check_error(data, expect)
    except OTAError as err:
        sock.close()
        # type(err) preserves OTANetworkError vs OTAError so callers can tell
        # retryable network failures from device-reported errors; subclasses
        # must accept a single message argument
        raise type(err)(f"receiving {msg}: {err}") from err

    while len(data) < amount:
        try:
            data += recv_decode(sock, amount - len(data), decode=decode)  # type: ignore[operator]
        except OSError as err:
            raise OTANetworkError(f"receiving {msg}: {err}") from err
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
        raise OTANetworkError(
            "Device closed connection without responding. "
            "This may indicate the device ran out of memory, "
            "a network issue, or the connection was interrupted."
        )
    dat = data[0]
    error_msg = _ERROR_MESSAGES.get(dat)
    if error_msg is not None:
        raise OTAError(error_msg)
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
        raise OTANetworkError(f"sending {msg}: {err}") from err


class NoiseSocketWrapper:
    """Runs the OTA session inside a Noise (ChaCha20-Poly1305) transport.

    Exposes the socket subset perform_ota uses. Frames are indicator 0x01,
    16-bit big-endian length, ciphertext; recv() drains one decrypted frame
    at a time, sendall() keeps control units in one frame and splits data
    at NOISE_MAX_PLAINTEXT.
    """

    def __init__(self, sock: socket.socket, psk: str, prologue: bytes) -> None:
        # Deliberately lazy: the noise stack (noiseprotocol, cryptography) is
        # only imported when an encrypted upload actually runs.
        try:
            from aioesphomeapi.noise import NoiseHandshake
        except ImportError as err:
            raise OTAError(
                "OTA encryption requires a newer aioesphomeapi; update your "
                "esphome installation (pip install -U esphome) and retry"
            ) from err
        # The aioesphomeapi import above already loaded cryptography; bind
        # the exception once so recv() pays no per-frame import lookup
        from cryptography.exceptions import InvalidTag

        self._invalid_tag = InvalidTag
        self._sock = sock
        try:
            self._handshake = NoiseHandshake(psk, prologue)
        except ValueError as err:
            raise OTAError(f"Invalid OTA encryption key: {err}") from err
        self._encrypt = None
        self._decrypt = None
        self._buffer = b""

    # Only harmless socket controls pass through; byte-moving methods are
    # deliberately absent so plaintext cannot leak past the transport.
    def settimeout(self, timeout: float | None) -> None:
        self._sock.settimeout(timeout)

    def setsockopt(self, level: int, optname: int, value: int) -> None:
        self._sock.setsockopt(level, optname, value)

    def close(self) -> None:
        self._sock.close()

    def do_handshake(self) -> None:
        """Run the two-message NNpsk0 handshake and set up the transport ciphers."""
        try:
            self._send_frame(
                bytes([NOISE_HANDSHAKE_OK]) + self._handshake.write_message()
            )
            payload = self._recv_frame()
        except OSError as err:
            raise OTANetworkError(f"noise handshake: {err}") from err
        if not payload:
            raise OTANetworkError("Device closed connection during the noise handshake")
        if payload[0] != NOISE_HANDSHAKE_OK:
            reason = payload[1:].decode("utf-8", "replace")
            if reason == NOISE_MAC_FAILURE_REASON:
                raise OTAError(
                    "Device rejected the handshake; is the OTA encryption key correct?"
                )
            raise OTAError(f"Device rejected the noise handshake: {reason}")
        try:
            self._handshake.read_message(payload[1:])
        except (ValueError, self._invalid_tag) as err:
            # InvalidTag is a wrong key; ValueError covers a device sending an
            # invalid curve point, which cryptography rejects during the DH
            raise OTAError(
                "Noise handshake failed; is the OTA encryption key correct?"
            ) from err
        self._encrypt, self._decrypt = self._handshake.get_ciphers()

    def sendall(self, data: bytes) -> None:
        frames: list[bytes] = []
        for offset in range(0, len(data), NOISE_MAX_PLAINTEXT):
            ciphertext = self._encrypt.encrypt(
                data[offset : offset + NOISE_MAX_PLAINTEXT]
            )
            frames.append(self._frame_header(len(ciphertext)))
            frames.append(ciphertext)
        self._sock.sendall(b"".join(frames))

    def recv(self, amount: int) -> bytes:
        if not self._buffer:
            ciphertext = self._recv_frame()
            if not ciphertext:
                return b""  # connection closed at a frame boundary
            try:
                self._buffer = self._decrypt.decrypt(ciphertext)
            except self._invalid_tag as err:
                # Retryable: a fresh connection renegotiates the session
                raise OTANetworkError(
                    "Noise decryption failed (MAC mismatch); frame corrupted or tampered"
                ) from err
            if not self._buffer:
                # Reject MAC-only frames so b"" always means the peer closed
                raise OTANetworkError("Device sent an empty noise frame")
        data = self._buffer[:amount]
        self._buffer = self._buffer[amount:]
        return data

    @staticmethod
    def _frame_header(length: int) -> bytes:
        return bytes([NOISE_FRAME_INDICATOR, (length >> 8) & 0xFF, length & 0xFF])

    def _send_frame(self, payload: bytes) -> None:
        self._sock.sendall(self._frame_header(len(payload)) + payload)

    def _recv_frame(self) -> bytes:
        header = self._recv_exact(3, closed_ok=True)
        if not header:
            return b""  # connection closed at a frame boundary
        # A malformed frame is a broken transport, not a device error;
        # retryable so a fresh session is tried
        if header[0] != NOISE_FRAME_INDICATOR:
            raise OTANetworkError(f"Bad noise frame indicator 0x{header[0]:02X}")
        length = (header[1] << 8) | header[2]
        if length == 0:
            raise OTANetworkError("Device sent an empty noise frame")
        return self._recv_exact(length)

    def _recv_exact(self, amount: int, closed_ok: bool = False) -> bytes:
        data = b""
        while len(data) < amount:
            chunk = self._sock.recv(amount - len(data))
            if not chunk:
                if closed_ok and not data:
                    return b""
                raise OSError("connection closed inside a noise frame")
            data += chunk
        return data


def perform_ota(
    sock: socket.socket,
    password: str | None,
    file_handle: io.IOBase,
    filename: Path,
    ota_type: int = OTA_TYPE_UPDATE_APP,
    noise_psk: str | None = None,
) -> None:
    # Validate up front; an out-of-range value would only surface as a
    # ValueError deep inside send_check, bypassing OTAError handling
    if not isinstance(ota_type, int) or not 0 <= ota_type <= 0xFF:
        raise OTAError(
            f"Invalid ota_type {ota_type!r}; expected an integer in range 0-255"
        )
    if ota_type not in _SUPPORTED_OTA_TYPES:
        supported = ", ".join(f"0x{t:02X}" for t in sorted(_SUPPORTED_OTA_TYPES))
        raise OTAError(
            f"Unsupported OTA type 0x{ota_type:02X}; this ESPHome supports: {supported}"
        )

    if noise_psk is not None and not noise_psk:
        raise OTAError(
            "An empty OTA encryption key was provided; refusing to upload in plaintext"
        )

    file_contents = file_handle.read()
    file_size = len(file_contents)
    _LOGGER.info("Uploading %s (%s bytes)", filename, file_size)

    # Enable nodelay, we need it for phase 1
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    send_check(sock, MAGIC_BYTES, "magic bytes")

    _, version = receive_exactly(sock, 2, "version", RESPONSE_OK)
    _LOGGER.info("Connection established; device supports OTA version %s", version)
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
    if noise_psk:
        features_to_send |= CLIENT_FEATURE_SUPPORTS_NOISE
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

    if noise_psk:
        # Fail closed: never fall back to a plaintext upload when an
        # encryption key is configured, an active attacker could otherwise
        # strip the feature flag and capture the image (it contains the wifi
        # credentials and the api encryption key).
        if not (extended_proto and features & SERVER_FEATURE_SUPPORTS_NOISE):
            raise OTAError(
                "An OTA encryption key is configured but the device did not "
                "offer encryption; refusing to send the image in plaintext. "
                "If the running firmware predates OTA encryption, first update "
                "it without the 'ota: encryption:' block (over a trusted "
                "network or via USB), then restore the block and upload again."
            )
        # The prologue binds every negotiation byte both sides saw, so any
        # tampering with the plaintext preamble breaks the handshake.
        prologue = (
            NOISE_PROLOGUE_INIT
            + bytes(MAGIC_BYTES)
            + bytes([RESPONSE_OK, version, features_to_send])
            + bytes([RESPONSE_FEATURE_FLAGS, features])
        )
        sock = NoiseSocketWrapper(sock, noise_psk, prologue)
        sock.do_handshake()
        _LOGGER.info("Encrypted connection established")

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
            sock, nonce_size, f"{hash_name} auth nonce", None, decode=False
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

    _LOGGER.info("Handshake complete")

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
    # The device erases flash between receiving the size and acking the
    # prepare, so this window shows the erase cost (near zero when the
    # device erases lazily during the upload)
    prepare_start = time.perf_counter()
    send_check(sock, upload_size_encoded, "binary size")
    receive_exactly(sock, 1, "update prepare result", RESPONSE_UPDATE_PREPARE_OK)
    prepare_duration = time.perf_counter() - prepare_start
    _LOGGER.info("Preparing for upload took %.2f seconds", prepare_duration)

    upload_md5 = hashlib.md5(upload_contents).hexdigest()
    _LOGGER.debug("MD5 of upload is %s", upload_md5)

    send_check(sock, upload_md5, "file checksum")
    receive_exactly(sock, 1, "file checksum result", RESPONSE_BIN_MD5_OK)

    # Disable nodelay for transfer
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 0)
    # Limit send buffer (usually around 100kB) in order to have progress bar
    # show the actual progress

    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, UPLOAD_BUFFER_SIZE)
    start_time = time.perf_counter()

    offset = 0
    progress = ProgressBar("Uploading")
    try:
        while True:
            chunk = upload_contents[offset : offset + UPLOAD_BLOCK_SIZE]
            if not chunk:
                break
            offset += len(chunk)

            try:
                sock.sendall(chunk)
            except OSError as err:
                # A send failure can hide an error byte the device reported
                # just before dropping the connection; surface that as the
                # real, non-retryable cause when it is available
                try:
                    sock.settimeout(1.0)
                    check_error(recv_decode(sock, 1), None)
                except (OSError, OTANetworkError) as probe_err:
                    _LOGGER.debug(
                        "No device error behind the send failure: %s", probe_err
                    )
                raise OTANetworkError(f"sending data: {err}") from err

            if version >= OTA_VERSION_2_0:
                try:
                    receive_exactly(sock, 1, "chunk result", RESPONSE_CHUNK_OK)
                except OTANetworkError as err:
                    if offset < upload_size:
                        raise
                    # The device already had the complete image when this ack
                    # was lost, so it may be committing; do not retry
                    raise _committed_error(err) from err

            progress.update(offset / upload_size)
    except OTAError:
        # Terminate the progress bar line before the error is logged
        progress.done()
        raise
    progress.done()

    # Enable nodelay for last checks
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    duration = time.perf_counter() - start_time

    _LOGGER.info("Upload took %.2f seconds, waiting for result...", duration)

    # Once the device has the complete image it commits the update and
    # reboots on its own; the exact commit point is not observable from
    # here, so treat everything past the data phase as non-retryable. A
    # re-upload could flash a device that already updated successfully.
    commit_start = time.perf_counter()
    try:
        receive_exactly(sock, 1, "update receive result", RESPONSE_RECEIVE_OK)
        receive_exactly(sock, 1, "update end result", RESPONSE_UPDATE_END_OK)
    except OTANetworkError as err:
        raise _committed_error(err) from err
    commit_duration = time.perf_counter() - commit_start

    # Sum of the named windows so the breakdown is self consistent; connect,
    # handshake, auth, and the one MD5 round trip are not included
    _LOGGER.info(
        "Update took %.2f seconds (prepare %.2f, upload %.2f, commit %.2f)",
        prepare_duration + duration + commit_duration,
        prepare_duration,
        duration,
        commit_duration,
    )

    try:
        send_check(sock, RESPONSE_OK, "end acknowledgement")
    except OTANetworkError as err:
        # The device treats a missing end acknowledgement as non-fatal and is
        # already rebooting into the new firmware, so the update succeeded
        _LOGGER.warning("Failed sending end acknowledgement: %s", err)
        _LOGGER.info("OTA successful (end acknowledgement not delivered)")
    else:
        _LOGGER.info("OTA successful")

    # Do not connect logs until it is fully on
    time.sleep(1)


def run_ota_impl_(
    remote_host: str | list[str],
    remote_port: int,
    password: str | None,
    filename: Path,
    ota_type: int = OTA_TYPE_UPDATE_APP,
    noise_psk: str | None = None,
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

    if not res:
        _LOGGER.error("No addresses to connect to for %s", remote_host)
        return 1, None

    # Every address is tried at least once and EXTRA_UPLOAD_ATTEMPTS retries
    # are shared across the addresses, cycling through them. Wait before an
    # attempt when the previous one actually reached the device, or when
    # revisiting an address, so a flaky link can recover and the device can
    # clean up a half-open connection (its handshake watchdog runs at 20s);
    # moving on to the next address family stays immediate. Known limitation:
    # a silent mid-transfer drop with no reset can wedge the device until its
    # 90s data timeout, which outlasts this budget; the retries target the
    # common failures where the device resets or closes the link promptly.
    total_attempts = len(res) + EXTRA_UPLOAD_ATTEMPTS
    last_error = ""
    reached_device = False
    for attempt in range(total_attempts):
        af, socktype, _, _, sa = res[attempt % len(res)]
        if reached_device or attempt >= len(res):
            _LOGGER.info(
                "Retrying in %.0f seconds (attempt %d of %d)...",
                UPLOAD_RETRY_DELAY,
                attempt + 1,
                total_attempts,
            )
            time.sleep(UPLOAD_RETRY_DELAY)
        reached_device = False
        _LOGGER.info("Connecting to %s port %s...", sa[0], sa[1])
        sock = socket.socket(af, socktype)
        sock.settimeout(20.0)
        try:
            sock.connect(sa)
        except OSError as err:
            sock.close()
            _LOGGER.warning("Connecting to %s port %s failed: %s", sa[0], sa[1], err)
            last_error = f"connecting to {sa[0]} failed: {err}"
            continue

        _LOGGER.info("Connected to %s", sa[0])
        reached_device = True
        with contextlib.closing(sock), Path(filename).open("rb") as file_handle:
            try:
                perform_ota(sock, password, file_handle, filename, ota_type, noise_psk)
            except OTANetworkError as err:
                # Transient network failure; retry
                last_error = str(err)
                _LOGGER.warning("%s", last_error)
                continue
            except OTAError as err:
                # Device-reported error (wrong password, wrong flash size, ...);
                # retrying cannot succeed, so fail immediately
                _LOGGER.error(str(err))
                return 1, None

        # Successfully uploaded to sa[0]
        return 0, sa[0]

    _LOGGER.error("Upload failed after %d attempts: %s", total_attempts, last_error)
    return 1, None


def run_ota(
    remote_host: str | list[str],
    remote_port: int,
    password: str | None,
    filename: Path,
    ota_type: int = OTA_TYPE_UPDATE_APP,
    noise_psk: str | None = None,
) -> tuple[int, str | None]:
    try:
        return run_ota_impl_(
            remote_host, remote_port, password, filename, ota_type, noise_psk
        )
    except OTAError as err:
        _LOGGER.error(err)
        return 1, None
