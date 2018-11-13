import hashlib
import logging
import random
import socket
import sys

from esphomeyaml.core import ESPHomeYAMLError

RESPONSE_OK = 0
RESPONSE_REQUEST_AUTH = 1

RESPONSE_HEADER_OK = 64
RESPONSE_AUTH_OK = 65
RESPONSE_UPDATE_PREPARE_OK = 66
RESPONSE_BIN_MD5_OK = 67
RESPONSE_RECEIVE_OK = 68
RESPONSE_UPDATE_END_OK = 69

RESPONSE_ERROR_MAGIC = 128
RESPONSE_ERROR_UPDATE_PREPARE = 129
RESPONSE_ERROR_AUTH_INVALID = 130
RESPONSE_ERROR_WRITING_FLASH = 131
RESPONSE_ERROR_UPDATE_END = 132
RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 133
RESPONSE_ERROR_UNKNOWN = 255

OTA_VERSION_1_0 = 1

MAGIC_BYTES = [0x6C, 0x26, 0xF7, 0x5C, 0x45]

_LOGGER = logging.getLogger(__name__)
LAST_PROGRESS = -1


def update_progress(progress):
    global LAST_PROGRESS

    bar_length = 60
    status = ""
    if progress >= 1:
        progress = 1
        status = "Done...\r\n"
    new_progress = int(progress * 100)
    if new_progress == LAST_PROGRESS:
        return
    LAST_PROGRESS = new_progress
    block = int(round(bar_length * progress))
    text = "\rUploading: [{0}] {1}% {2}".format("=" * block + " " * (bar_length - block),
                                                new_progress, status)
    sys.stderr.write(text)
    sys.stderr.flush()


class OTAError(ESPHomeYAMLError):
    pass


def recv_decode(sock, amount, decode=True):
    data = sock.recv(amount)
    if not decode:
        return data
    return [ord(x) for x in data]


def receive_exactly(sock, amount, msg, expect, decode=True):
    if decode:
        data = []
    else:
        data = ''

    try:
        data += recv_decode(sock, 1, decode=decode)
    except socket.error as err:
        raise OTAError("Error receiving acknowledge {}: {}".format(msg, err))

    try:
        check_error(data, expect)
    except OTAError as err:
        sock.close()
        raise OTAError("Error {}: {}".format(msg, err))

    while len(data) < amount:
        try:
            data += recv_decode(sock, amount - len(data), decode=decode)
        except socket.error as err:
            raise OTAError("Error receiving {}: {}".format(msg, err))
    return data


def check_error(data, expect):
    if not expect:
        return
    dat = data[0]
    if dat == RESPONSE_ERROR_MAGIC:
        raise OTAError("Error: Invalid magic byte")
    if dat == RESPONSE_ERROR_UPDATE_PREPARE:
        raise OTAError("Error: Couldn't prepare flash memory for update. Is the binary too big? "
                       "Please try restarting the ESP.")
    if dat == RESPONSE_ERROR_AUTH_INVALID:
        raise OTAError("Error: Authentication invalid. Is the password correct?")
    if dat == RESPONSE_ERROR_WRITING_FLASH:
        raise OTAError("Error: Wring OTA data to flash memory failed. See USB logs for more "
                       "information.")
    if dat == RESPONSE_ERROR_UPDATE_END:
        raise OTAError("Error: Finishing update failed. See the MQTT/USB logs for more "
                       "information.")
    if dat == RESPONSE_ERROR_INVALID_BOOTSTRAPPING:
        raise OTAError("Error: Please press the reset button on the ESP. A manual reset is "
                       "required on the first OTA-Update after flashing via USB.")
    if dat == RESPONSE_ERROR_UNKNOWN:
        raise OTAError("Unknown error from ESP")
    if not isinstance(expect, (list, tuple)):
        expect = [expect]
    if dat not in expect:
        raise OTAError("Unexpected response from ESP: 0x{:02X}".format(data[0]))


def send_check(sock, data, msg):
    try:
        if isinstance(data, (list, tuple)):
            data = ''.join([chr(x) for x in data])
        elif isinstance(data, int):
            data = chr(data)
        sock.sendall(data)
    except socket.error as err:
        raise OTAError("Error sending {}: {}".format(msg, err))


def perform_ota(sock, password, file_handle, filename):
    file_md5 = hashlib.md5(file_handle.read()).hexdigest()
    file_size = file_handle.tell()
    _LOGGER.info('Uploading %s (%s bytes)', filename, file_size)
    file_handle.seek(0)
    _LOGGER.debug("MD5 of binary is %s", file_md5)

    # Enable nodelay, we need it for phase 1
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    send_check(sock, MAGIC_BYTES, 'magic bytes')

    _, version = receive_exactly(sock, 2, 'version', RESPONSE_OK)
    if version != OTA_VERSION_1_0:
        raise OTAError("Unsupported OTA version {}".format(version))

    # Features
    send_check(sock, 0x00, 'features')
    receive_exactly(sock, 1, 'features', RESPONSE_HEADER_OK)

    auth, = receive_exactly(sock, 1, 'auth', [RESPONSE_REQUEST_AUTH, RESPONSE_AUTH_OK])
    if auth == RESPONSE_REQUEST_AUTH:
        if not password:
            raise OTAError("ESP requests password, but no password given!")
        nonce = receive_exactly(sock, 32, 'authentication nonce', [], decode=False)
        _LOGGER.debug("Auth: Nonce is %s", nonce)
        cnonce = hashlib.md5(str(random.random()).encode()).hexdigest()
        _LOGGER.debug("Auth: CNonce is %s", cnonce)

        send_check(sock, cnonce, 'auth cnonce')

        result_md5 = hashlib.md5()
        result_md5.update(password.encode())
        result_md5.update(nonce.encode())
        result_md5.update(cnonce.encode())
        result = result_md5.hexdigest()
        _LOGGER.debug("Auth: Result is %s", result)

        send_check(sock, result, 'auth result')
        receive_exactly(sock, 1, 'auth result', RESPONSE_AUTH_OK)
    else:
        if password:
            raise OTAError("Password specified, but ESP doesn't accept password!")

    file_size_encoded = [
        (file_size >> 24) & 0xFF,
        (file_size >> 16) & 0xFF,
        (file_size >> 8) & 0xFF,
        (file_size >> 0) & 0xFF,
    ]
    send_check(sock, file_size_encoded, 'binary size')
    receive_exactly(sock, 1, 'binary size', RESPONSE_UPDATE_PREPARE_OK)

    send_check(sock, file_md5, 'file checksum')
    receive_exactly(sock, 1, 'file checksum', RESPONSE_BIN_MD5_OK)

    # Disable nodelay for transfer
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 0)
    # Limit send buffer (usually around 100kB) in order to have progress bar
    # show the actual progress
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 8192)

    offset = 0
    update_progress(0.0)
    while True:
        chunk = file_handle.read(1024)
        if not chunk:
            break
        offset += len(chunk)

        try:
            sock.sendall(chunk)
        except socket.error as err:
            sys.stderr.write('\n')
            raise OTAError("Error sending data: {}".format(err))

        update_progress(offset / float(file_size))

    # Enable nodelay for last checks
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    sys.stderr.write('\n')
    _LOGGER.info("Waiting for result...")

    receive_exactly(sock, 1, 'receive OK', RESPONSE_RECEIVE_OK)
    receive_exactly(sock, 1, 'Update end', RESPONSE_UPDATE_END_OK)
    send_check(sock, RESPONSE_OK, 'end acknowledgement')

    _LOGGER.info("OTA successful")


def is_ip_address(host):
    parts = host.split('.')
    if len(parts) != 4:
        return False
    try:
        for p in parts:
            int(p)
        return True
    except ValueError:
        return False


def resolve_ip_address(host):
    if is_ip_address(host):
        _LOGGER.info("Connecting to %s", host)
        return host

    _LOGGER.info("Resolving IP Address of %s", host)
    hosts = [host]
    if host.endswith('.local'):
        hosts.append(host[:-6])

    errors = []
    for x in hosts:
        try:
            ip = socket.gethostbyname(x)
            break
        except socket.error as err:
            errors.append(err)
    else:
        _LOGGER.error("Error resolving IP address of %s. Is it connected to WiFi?",
                      host)

        _LOGGER.error("(If this error persists, please set a static IP address: "
                      "https://esphomelib.com/esphomeyaml/components/wifi.html#manual-ips)")
        raise OTAError("Errors: {}".format(', '.join(str(x) for x in errors)))

    _LOGGER.info(" -> %s", ip)
    return ip


def run_ota(remote_host, remote_port, password, filename):
    ip = resolve_ip_address(remote_host)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10.0)
    try:
        sock.connect((ip, remote_port))
    except socket.error as err:
        sock.close()
        _LOGGER.error("Connecting to %s:%s failed: %s", remote_host, remote_port, err)
        return 1

    file_handle = open(filename, 'rb')
    try:
        perform_ota(sock, password, file_handle, filename)
    except OTAError as err:
        _LOGGER.error(str(err))
        return 1
    finally:
        sock.close()
        file_handle.close()

    return 0


def run_legacy_ota(verbose, host_port, remote_host, remote_port, password, filename):
    from esphomeyaml import espota

    espota_args = ['espota.py', '--debug', '--progress', '-i', remote_host,
                   '-p', str(remote_port), '-f', filename,
                   '-a', password, '-P', str(host_port)]
    if verbose:
        espota_args.append('-d')
    return espota.main(espota_args)
