import hashlib
import logging
import random
import socket
import sys
import time
import gzip
import struct

from dataclasses import dataclass
from esphome.core import EsphomeError
from esphome.helpers import is_ip_address, resolve_ip_address

UPLOAD_TYPE_APP = 1
UPLOAD_TYPE_BOOTLOADER = 2
UPLOAD_TYPE_PARTITION_TABLE = 3
UPLOAD_TYPE_PARTITION = 4

COMMAND_WRITE = 1
COMMAND_REBOOT = 2
COMMAND_END = 3
COMMAND_READ = 4

RESPONSE_OK = 0
RESPONSE_REQUEST_AUTH = 1
RESPONSE_REQUEST_MD5 = 2

RESPONSE_HEADER_OK = 64
RESPONSE_AUTH_OK = 65
RESPONSE_UPDATE_PREPARE_OK = 66
RESPONSE_BIN_MD5_OK = 67
RESPONSE_RECEIVE_OK = 68
RESPONSE_UPDATE_END_OK = 69
RESPONSE_SUPPORTS_COMPRESSION = 70
RESPONSE_SUPPORTS_EXTENDED = 71
RESPONSE_READ_PREPARE_OK = (72,)
RESPONSE_READ_END_OK = (73,)

RESPONSE_ERROR_MAGIC = 128
RESPONSE_ERROR_UPDATE_PREPARE = 129
RESPONSE_ERROR_AUTH_INVALID = 130
RESPONSE_ERROR_WRITING_FLASH = 131
RESPONSE_ERROR_UPDATE_END = 132
RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 133
RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG = 134
RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG = 135
RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE = 136
RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE = 137
RESPONSE_ERROR_NO_UPDATE_PARTITION = 138
RESPONSE_ERROR_MD5_MISMATCH = 139
RESPONSE_ERROR_RP2040_NOT_ENOUGH_SPACE = 140
RESPONSE_ERROR_UNKNOWN_PARTITION_INFO_VERSION = 141
RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED = 142
RESPONSE_ERROR_ESP32_REGISTERING_PARTITION = 143
RESPONSE_ERROR_PARTITION_NOT_FOUND = 144
RESPONSE_ERROR_UNKNOWN_COMMAND = 145
RESPONSE_ERROR_ABORT_OVERRIDE = 146
RESPONSE_ERROR_UNKNOWN = 255

OTA_VERSION_1_0 = 1

MAGIC_BYTES = [0x6C, 0x26, 0xF7, 0x5C, 0x45]

FEATURE_QUERY_SUPPORTS_COMPRESSION = 0x01
FEATURE_QUERY_SUPPORTS_EXTENDED = 0x02

FEATURE_SUPPORTS_COMPRESSION = 0x01
FEATURE_SUPPORTS_WRITING_BOOTLOADER = 0x02
FEATURE_SUPPORTS_WRITING_PARTITION_TABLE = 0x03
FEATURE_SUPPORTS_WRITING_PARTITIONS = 0x04
FEATURE_SUPPORTS_READING = 0x05

_LOGGER = logging.getLogger(__name__)


@dataclass
class OTAPartitionType:
    type: int

    @dataclass
    class partition:
        """Partition information for flash"""

        type: int = 0xFF
        subtype: int = 0xFF
        index: int = 0
        label: str = ""


class ProgressBar:
    def __init__(self, prefix="Uploading"):
        self.last_progress = None
        self.prefix = prefix

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
        text = f"\r{self.prefix}: [{'=' * block + ' ' * (bar_length - block)}] {new_progress}% {status}"
        sys.stderr.write(text)
        sys.stderr.flush()

    def done(self):
        sys.stderr.write("\n")
        sys.stderr.flush()


class OTAError(EsphomeError):
    pass


def recv_decode(sock, amount, decode=True):
    data = sock.recv(amount)
    if not decode:
        return data
    return list(data)


def receive_exactly(sock, amount, msg, expect, decode=True):
    if decode:
        data = []
    else:
        data = b""

    try:
        data += recv_decode(sock, 1, decode=decode)
    except OSError as err:
        raise OTAError(f"Error receiving acknowledge {msg}: {err}") from err

    try:
        check_error(data, expect)
    except OTAError as err:
        sock.close()
        raise OTAError(f"Error {msg}: {err}") from err

    while len(data) < amount:
        try:
            data += recv_decode(sock, amount - len(data), decode=decode)
        except OSError as err:
            raise OTAError(f"Error receiving {msg}: {err}") from err
    return data


def check_error(data, expect):
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
            "Error: Wring OTA data to flash memory failed. See USB logs for more "
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
    if dat == RESPONSE_ERROR_RP2040_NOT_ENOUGH_SPACE:
        raise OTAError(
            "Error: The OTA partition on the RP2040 is too small. ESPHome needs to resize "
            "this partition, please flash over USB."
        )
    if dat == RESPONSE_ERROR_UNKNOWN_PARTITION_INFO_VERSION:
        raise OTAError(
            "Error: The device firmware is too old and does not support this type of OTA. Try "
            "to update first."
        )
    if dat == RESPONSE_ERROR_UNKNOWN_PARTITION_INFO_VERSION:
        raise OTAError(
            "Error: The device firmware is too old and does not support this type of OTA. Try "
            "a regular OTA update first or flash over USB."
        )
    if dat == RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED:
        raise OTAError(
            "Error: The device does not support flashing this type of partition. Check if the "
            "framework and CPU you are using supports it."
        )
    if dat == RESPONSE_ERROR_ESP32_REGISTERING_PARTITION:
        raise OTAError(
            "Error: could not register partition before flashing. Try to reboot your device first"
        )
    if dat == RESPONSE_ERROR_PARTITION_NOT_FOUND:
        raise OTAError(
            "Error: could not find the custom partition to flash. Did you specify the right values?"
        )
    if dat == RESPONSE_ERROR_UNKNOWN_COMMAND:
        raise OTAError(
            "Error: device does not support the sent command. This means the device firmware is too "
            "old and this CLI did not check the device features properly."
        )
    if dat == RESPONSE_ERROR_ABORT_OVERRIDE:
        raise OTAError(
            "Error: device aborted flash that would have overriden running partition. Check your "
            "device log for more information."
        )
    if dat == RESPONSE_ERROR_UNKNOWN:
        raise OTAError("Unknown error from ESP")
    if not isinstance(expect, (list, tuple)):
        expect = [expect]
    if dat not in expect:
        raise OTAError(f"Unexpected response from ESP: 0x{data[0]:02X}")


def send_check(sock, data, msg):
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


def perform_ota(sock, password, file_handle, bin_type, filename, no_reboot, is_upload):
    # Enable nodelay, we need it for phase 1
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    send_check(sock, MAGIC_BYTES, "magic bytes")

    _, version = receive_exactly(sock, 2, "version", RESPONSE_OK)
    if version != OTA_VERSION_1_0:
        raise OTAError(f"Unsupported OTA version {version}")

    # Query features
    send_check(
        sock,
        FEATURE_QUERY_SUPPORTS_COMPRESSION + FEATURE_QUERY_SUPPORTS_EXTENDED,
        "features",
    )
    features_reply = receive_exactly(
        sock,
        1,
        "features",
        [RESPONSE_HEADER_OK, RESPONSE_SUPPORTS_COMPRESSION, RESPONSE_SUPPORTS_EXTENDED],
    )[0]

    # Check feature reply and set features
    features = []
    if features_reply == RESPONSE_SUPPORTS_COMPRESSION:
        features = [FEATURE_SUPPORTS_COMPRESSION]
    if features_reply == RESPONSE_SUPPORTS_EXTENDED:
        enabled_features = receive_exactly(sock, 1, "extended features legth", False)[0]
        if enabled_features > 0:
            features = receive_exactly(
                sock, enabled_features, "extended features", False
            )
    _LOGGER.debug("Features: %s", features)

    if (bin_type.type == UPLOAD_TYPE_BOOTLOADER) and (
        FEATURE_SUPPORTS_WRITING_BOOTLOADER not in features
    ):
        raise OTAError(
            "Transfering bootloader is not supported by remote device - upgrade the firmware first!"
        )

    if (bin_type.type == UPLOAD_TYPE_PARTITION_TABLE) and (
        FEATURE_SUPPORTS_WRITING_PARTITION_TABLE not in features
    ):
        raise OTAError(
            "Transfering partition table is not supported by remote device - upgrade the firmware first!"
        )

    if (bin_type.type == UPLOAD_TYPE_PARTITION) and (
        FEATURE_SUPPORTS_WRITING_PARTITIONS not in features
    ):
        raise OTAError(
            "Transfering partition is not supported by remote device - upgrade the firmware first!"
        )

    if (not is_upload) and (FEATURE_SUPPORTS_READING not in features):
        raise OTAError(
            "Reading firmware is not supported by remote device - upgrade the firmware first!"
        )

    (auth,) = receive_exactly(
        sock, 1, "auth", [RESPONSE_REQUEST_AUTH, RESPONSE_AUTH_OK]
    )
    if auth == RESPONSE_REQUEST_AUTH:
        if not password:
            raise OTAError("ESP requests password, but no password given!")
        nonce = receive_exactly(
            sock, 32, "authentication nonce", [], decode=False
        ).decode()
        _LOGGER.debug("Auth: Nonce is %s", nonce)
        cnonce = hashlib.md5(str(random.random()).encode()).hexdigest()
        _LOGGER.debug("Auth: CNonce is %s", cnonce)

        send_check(sock, cnonce, "auth cnonce")

        result_md5 = hashlib.md5()
        result_md5.update(password.encode("utf-8"))
        result_md5.update(nonce.encode())
        result_md5.update(cnonce.encode())
        result = result_md5.hexdigest()
        _LOGGER.debug("Auth: Result is %s", result)

        send_check(sock, result, "auth result")
        receive_exactly(sock, 1, "auth result", RESPONSE_AUTH_OK)

    if is_upload:
        file_contents = file_handle.read()
        file_size = len(file_contents)
        _LOGGER.info("Uploading %s (%s bytes)", filename, file_size)

        if FEATURE_SUPPORTS_COMPRESSION in features:
            transfer_contents = gzip.compress(file_contents, compresslevel=9)
            _LOGGER.info("Compressed to %s bytes", len(transfer_contents))
        else:
            transfer_contents = file_contents

        upload_size = len(transfer_contents)

        send_check(sock, struct.pack("!B", COMMAND_WRITE), "write command")
    else:
        # Read
        upload_size = 0
        send_check(sock, struct.pack("!B", COMMAND_READ), "read command")

    if features_reply == RESPONSE_SUPPORTS_EXTENDED:
        # Send partition info
        # - [ 0   ] version: 0x1
        # - [ 1   ] bin type (if version >= 1)
        # - [ 2- 5] bin length (if version >= 1)
        # - [ 6   ] partition type - when bin type = partition (if version >= 1)
        # - [ 7   ] partition subtype - when bin type = partition (if version >= 1)
        # - [ 8   ] partition index - when bin type = partition (if version >= 1)
        # - [16-31] partition label - when bin type = partition (if version >= 1)
        version = 1
        partition_label = bin_type.partition.label
        assert len(partition_label) <= 15
        partition_label_bytes = bytes(partition_label, "ascii") + b"\x00"
        partition_info = struct.pack(
            "!BBLBBB7x16s",
            version,
            bin_type.type,
            upload_size,
            bin_type.partition.type,
            bin_type.partition.subtype,
            bin_type.partition.index,
            partition_label_bytes,
        )
        assert len(partition_info) == 32
        _LOGGER.debug("Partition info: %s", list(partition_info))
        send_check(sock, partition_info, "partition info")
    else:
        if bin_type.type != UPLOAD_TYPE_APP:
            raise OTAError(
                "Error: The device firmware is too old and does not support anything else beyond "
                "the plain OTA. Try an update first."
            )
        upload_size_encoded = struct.pack("!L", upload_size)
        send_check(sock, upload_size_encoded, "binary size")
    if is_upload:
        receive_exactly(sock, 1, "binary size", RESPONSE_UPDATE_PREPARE_OK)

        upload_md5 = hashlib.md5(transfer_contents).hexdigest()
        _LOGGER.debug("MD5 of upload is %s", upload_md5)

        send_check(sock, upload_md5, "file checksum")
        receive_exactly(sock, 1, "file checksum", RESPONSE_BIN_MD5_OK)
    else:
        # Read
        read_prep_response = receive_exactly(
            sock, 5, "file size for read", RESPONSE_READ_PREPARE_OK
        )

        file_size = struct.unpack("!BL", bytes(read_prep_response))[1]
        _LOGGER.info("Downloading %s (%s bytes)", filename, file_size)

    # Disable nodelay for transfer
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 0)
    # Limit send buffer (usually around 100kB) in order to have progress bar
    # show the actual progress
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 8192)
    # Set higher timeout during upload
    sock.settimeout(20.0)

    offset = 0
    progress = ProgressBar("Uploading" if is_upload else "Downloading")
    while True:
        if is_upload:
            chunk = transfer_contents[offset : offset + 1024]
        else:
            chunk_size = min(1024, file_size - offset)
            chunk = (
                receive_exactly(sock, chunk_size, "receive firmware chunk", None)
                if chunk_size > 0
                else None
            )

        if not chunk:
            break
        offset += len(chunk)

        if is_upload:
            try:
                sock.sendall(chunk)
            except OSError as err:
                sys.stderr.write("\n")
                raise OTAError(f"Error sending data: {err}") from err
        else:
            file_handle.write(bytes(chunk))

        progress.update(offset / file_size)
    progress.done()

    # Enable nodelay for last checks
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    _LOGGER.info("Waiting for result...")

    if is_upload:
        receive_exactly(sock, 1, "receive OK", RESPONSE_RECEIVE_OK)
        receive_exactly(sock, 1, "Update end", RESPONSE_UPDATE_END_OK)
    else:
        # End Read

        file_handle.seek(0)
        download_md5 = hashlib.md5(file_handle.read()).hexdigest()
        _LOGGER.debug("MD5 of download is %s", download_md5)

        send_check(sock, download_md5, "file checksum")
        receive_exactly(sock, 1, "file checksum", RESPONSE_BIN_MD5_OK)

    if no_reboot:
        send_check(sock, COMMAND_END, "end without reboot")
    else:
        send_check(sock, COMMAND_REBOOT, "end with reboot")

    _LOGGER.info("OTA successful")

    # Do not connect logs until it is fully on
    time.sleep(1)


def run_ota_impl_(
    remote_host, remote_port, password, bin_type, filename, no_reboot, is_upload
):
    if is_ip_address(remote_host):
        _LOGGER.info("Connecting to %s", remote_host)
        ip = remote_host
    else:
        _LOGGER.info("Resolving IP address of %s", remote_host)
        try:
            ip = resolve_ip_address(remote_host)
        except EsphomeError as err:
            _LOGGER.error(
                "Error resolving IP address of %s. Is it connected to WiFi?",
                remote_host,
            )
            _LOGGER.error(
                "(If this error persists, please set a static IP address: "
                "https://esphome.io/components/wifi.html#manual-ips)"
            )
            raise OTAError(err) from err
        _LOGGER.info(" -> %s", ip)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10.0)
    try:
        sock.connect((ip, remote_port))
    except OSError as err:
        sock.close()
        _LOGGER.error("Connecting to %s:%s failed: %s", remote_host, remote_port, err)
        return 1

    open_mode = "rb" if is_upload else "wb+"
    with open(filename, open_mode) as file_handle:
        try:
            perform_ota(
                sock, password, file_handle, bin_type, filename, no_reboot, is_upload
            )
        except OTAError as err:
            _LOGGER.error(str(err))
            return 1
        finally:
            sock.close()

    return 0


def run_ota(remote_host, remote_port, password, bin_type, filename, no_reboot):
    try:
        return run_ota_impl_(
            remote_host, remote_port, password, bin_type, filename, no_reboot, True
        )
    except OTAError as err:
        _LOGGER.error(err)
        return 1


def download_ota(remote_host, remote_port, password, bin_type, filename):
    try:
        return run_ota_impl_(
            remote_host, remote_port, password, bin_type, filename, True, False
        )
    except OTAError as err:
        _LOGGER.error(err)
        return 1
