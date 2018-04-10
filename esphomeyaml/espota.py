#!/usr/bin/env python
#
# Copy of espota.py from ESP32 Arduino project with some modifications.
#
# Original espota.py by Ivan Grokhotkov:
# https://gist.github.com/igrr/d35ab8446922179dc58c
#
# Modified since 2015-09-18 from Pascal Gollor (https://github.com/pgollor)
# Modified since 2015-11-09 from Hristo Gochkov (https://github.com/me-no-dev)
# Modified since 2016-01-03 from Matthew O'Gorman (https://githumb.com/mogorman)
#
# This script will push an OTA update to the ESP
# use it like: python espota.py -i <ESP_IP_address> -I <Host_IP_address> -p <ESP_port> -P
# <Host_port> [-a password] -f <sketch.bin>
# Or to upload SPIFFS image:
# python espota.py -i <ESP_IP_address> -I <Host_IP_address> -p <ESP_port> -P <HOST_port> [-a
# password] -s -f <spiffs.bin>
#
# Changes
# 2018-03-29:
# - Clean up Code
# - Merge from esptool for ESP8266
# 2015-09-18:
# - Add option parser.
# - Add logging
# - Send command to controller to differ between flashing and transmitting SPIFFS image.
#
# Changes
# 2015-11-09:
# - Added digest authentication
# - Enhanced error tracking and reporting
#
# Changes
# 2016-01-03:
# - Added more options to parser.
#

import hashlib
import logging
import optparse
import os
import random
import socket
import sys

# pylint: disable=no-member

# Commands
FLASH = 0
SPIFFS = 100
AUTH = 200
PROGRESS = False

_LOGGER = logging.getLogger(__name__)


def update_progress(progress):
    """Displays or updates a console progress bar

    Accepts a float between 0 and 1. Any int will be converted to a float.
    A value under 0 represents a 'halt'. A value at 1 or bigger represents 100%.

    :param progress:
    :return:
    """
    if PROGRESS:
        bar_length = 60  # Modify this to change the length of the progress bar
        status = ""
        if isinstance(progress, int):
            progress = float(progress)
        if not isinstance(progress, float):
            progress = 0
            status = "error: progress var must be float\r\n"
        if progress < 0:
            progress = 0
            status = "Halt...\r\n"
        if progress >= 1:
            progress = 1
            status = "Done...\r\n"
        block = int(round(bar_length * progress))
        text = "\rUploading: [{0}] {1}% {2}".format("=" * block + " " * (bar_length - block),
                                                    int(progress * 100), status)
        sys.stderr.write(text)
        sys.stderr.flush()
    else:
        sys.stderr.write('.')
        sys.stderr.flush()


def serve(remote_host, local_addr, remote_port, local_port, password, filename, command=FLASH):
    # Create a TCP/IP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_address = (local_addr, local_port)
    _LOGGER.info('Starting on %s:%s', server_address[0], server_address[1])
    try:
        sock.bind(server_address)
        sock.listen(1)
    except Exception:  # pylint: disable=broad-except
        _LOGGER.error("Listen Failed")
        return 1

    content_size = os.path.getsize(filename)
    f_handle = open(filename, 'rb')
    file_md5 = hashlib.md5(f_handle.read()).hexdigest()
    f_handle.close()
    _LOGGER.info('Upload size: %d', content_size)
    message = '%d %d %d %s\n' % (command, local_port, content_size, file_md5)

    # Wait for a connection
    inv_trys = 0
    data = ''
    msg = 'Sending invitation to {} '.format(remote_host)
    _LOGGER.info(msg)
    remote_address = (remote_host, int(remote_port))
    sock2 = None
    while inv_trys < 10:
        inv_trys += 1
        sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock2.sendto(message.encode(), remote_address)
        except Exception:  # pylint: disable=broad-except
            _LOGGER.error('Failed')
            sock2.close()
            _LOGGER.error('Host %s Not Found', remote_host)
            return 1
        sock2.settimeout(1)
        try:
            data = sock2.recv(37).decode()
            break
        except Exception:  # pylint: disable=broad-except
            sys.stderr.write('.')
            sys.stderr.flush()
            sock2.close()
    sys.stderr.write('\n')
    sys.stderr.flush()
    if inv_trys == 10:
        _LOGGER.error('No response from the ESP')
        return 1
    if data != "OK":
        if data.startswith('AUTH'):
            nonce = data.split()[1]
            cnonce_text = '%s%u%s%s' % (filename, content_size, file_md5, remote_host)
            cnonce = hashlib.md5(cnonce_text.encode()).hexdigest()
            passmd5 = hashlib.md5(password.encode()).hexdigest()
            result_text = '%s:%s:%s' % (passmd5, nonce, cnonce)
            result = hashlib.md5(result_text.encode()).hexdigest()
            _LOGGER.info("Authenticating...")
            message = '%d %s %s\n' % (AUTH, cnonce, result)
            sock2.sendto(message.encode(), remote_address)
            sock2.settimeout(10)
            try:
                data = sock2.recv(32).decode()
            except Exception:  # pylint: disable=broad-except
                _LOGGER.error('FAIL: No Answer to our Authentication')
                sock2.close()
                return 1
            if data != "OK":
                _LOGGER.error('FAIL: %s', data)
                sock2.close()
                return 1
            _LOGGER.info('OK')
        else:
            _LOGGER.error('Bad Answer: %s', data)
            sock2.close()
            return 1
    sock2.close()

    _LOGGER.info('Waiting for device...')
    try:
        sock.settimeout(10)
        connection, _ = sock.accept()
        sock.settimeout(None)
        connection.settimeout(None)
    except Exception:  # pylint: disable=broad-except
        _LOGGER.error('No response from device')
        sock.close()
        return 1

    try:
        f_handle = open(filename, "rb")
        if PROGRESS:
            update_progress(0)
        else:
            _LOGGER.info('Uploading...')
        offset = 0
        while True:
            chunk = f_handle.read(1024)
            if not chunk:
                break
            offset += len(chunk)
            update_progress(offset / float(content_size))
            connection.settimeout(10)
            try:
                connection.sendall(chunk)
                connection.recv(10)
            except Exception:  # pylint: disable=broad-except
                sys.stderr.write('\n')
                _LOGGER.error('Error Uploading')
                connection.close()
                f_handle.close()
                sock.close()
                return 1

        sys.stderr.write('\n')
        _LOGGER.info('Waiting for result...')
        try:
            connection.settimeout(60)
            while True:
                if connection.recv(32).decode().find('O') >= 0:
                    break
            _LOGGER.info('Result: OK')
            connection.close()
            f_handle.close()
            sock.close()
            if data != "OK":
                _LOGGER.error('%s', data)
                return 1
        except Exception:  # pylint: disable=broad-except
            _LOGGER.error('No Result!')
            connection.close()
            f_handle.close()
            sock.close()
            return 1

    finally:
        connection.close()
        f_handle.close()

    return 0


def parse_args(unparsed_args):
    parser = optparse.OptionParser(
        usage="%prog [options]",
        description="Transmit image over the air to the esp8266 module with OTA support."
    )

    # destination ip and port
    group = optparse.OptionGroup(parser, "Destination")
    group.add_option(
        "-i", "--ip",
        dest="esp_ip",
        action="store",
        help="ESP8266 IP Address.",
        default=False
    )
    group.add_option(
        "-I", "--host_ip",
        dest="host_ip",
        action="store",
        help="Host IP Address.",
        default="0.0.0.0"
    )
    group.add_option(
        "-p", "--port",
        dest="esp_port",
        type="int",
        help="ESP8266 ota Port. Default 8266",
        default=8266
    )
    group.add_option(
        "-P", "--host_port",
        dest="host_port",
        type="int",
        help="Host server ota Port. Default random 10000-60000",
        default=random.randint(10000, 60000)
    )
    parser.add_option_group(group)

    # auth
    group = optparse.OptionGroup(parser, "Authentication")
    group.add_option(
        "-a", "--auth",
        dest="auth",
        help="Set authentication password.",
        action="store",
        default=""
    )
    parser.add_option_group(group)

    # image
    group = optparse.OptionGroup(parser, "Image")
    group.add_option(
        "-f", "--file",
        dest="image",
        help="Image file.",
        metavar="FILE",
        default=None
    )
    group.add_option(
        "-s", "--spiffs",
        dest="spiffs",
        action="store_true",
        help="Use this option to transmit a SPIFFS image and do not flash the "
             "module.",
        default=False
    )
    parser.add_option_group(group)

    # output group
    group = optparse.OptionGroup(parser, "Output")
    group.add_option(
        "-d", "--debug",
        dest="debug",
        help="Show debug output. And override loglevel with debug.",
        action="store_true",
        default=False
    )
    group.add_option(
        "-r", "--progress",
        dest="progress",
        help="Show progress output. Does not work for ArduinoIDE",
        action="store_true",
        default=False
    )
    parser.add_option_group(group)

    options, _ = parser.parse_args(unparsed_args)

    return options


def main(args):
    options = parse_args(args)
    _LOGGER.debug("Options: %s", str(options))

    # check options
    global PROGRESS
    PROGRESS = options.progress
    if not options.esp_ip or not options.image:
        _LOGGER.critical("Not enough arguments.")
        return 1

    command = FLASH
    if options.spiffs:
        command = SPIFFS

    return serve(options.esp_ip, options.host_ip, options.esp_port, options.host_port,
                 options.auth, options.image, command)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
