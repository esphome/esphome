# PYTHON_ARGCOMPLETE_OK
import argparse
import logging
import os
import sys
import time

import argcomplete
import click

from esphome import const
from esphome.config import read_config
from esphome.const import (
    CONF_ESPHOME,
    CONF_PLATFORMIO_OPTIONS,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
    SECRETS_FILES,
)
from esphome.core import CORE, EsphomeError
from esphome.log import Fore, color, setup_log
from esphome.util import (
    get_serial_ports,
    run_external_command,
    run_external_process,
    safe_print,
)

_LOGGER = logging.getLogger(__name__)


def choose_ports(selected: dict, purpose: str = None):
    click.echo(
        f'Found multiple options{f" for {purpose}" if purpose else ""}, please choose one:\n'
    )

    options = []
    for port in get_serial_ports():
        options.append((f"{port.path} ({port.description})", port.path))

    while True:
        for i, [desc, _] in enumerate(options):
            select = " "
            result = "          "
            if i in selected:
                select = color(Fore.BOLD_YELLOW, "X")
                if selected[i][1] is None:
                    result = "          "
                elif selected[i][1] == 0:
                    result = color(Fore.BOLD_GREEN, "SUCCESS")
                elif selected[i][1] > 0:
                    result = color(Fore.BOLD_RED, "FAILED")

            click.echo(f"     {i + 1}: [{select}] {desc}       {result}")

        click.echo("\nPort number [x = stop] [ENTER = continue]: ", None, False)
        opt = click.getchar(False)
        if opt in ["x", "X"]:
            click.echo(color(Fore.BOLD_BLUE, " Done"))
            return None
        if opt in ["\n", "\r", "\n\r", "\r\n"]:
            opt = 0
            click.echo(color(Fore.BOLD_BLUE, " Starting"))
            if len(selected) > 0:
                return selected
        try:
            opt = int(opt)
            if opt < 1 or opt > len(options):
                raise ValueError
        except ValueError:
            click.echo(
                color(Fore.RED, "\r       Invalid option                     "),
                None,
                False,
            )
            time.sleep(2)
            opt = 0
        if opt > 0:
            if opt - 1 in selected:
                selected.pop(opt - 1)
            else:
                selected[opt - 1] = [options[opt - 1][1], None]

        x = len(options) + 2
        click.echo(f"\033[{x}A")


def upload_using_esptool(config, port, file):
    from esphome import platformio_api

    first_baudrate = config[CONF_ESPHOME][CONF_PLATFORMIO_OPTIONS].get(
        "upload_speed", 115200
    )

    if file is not None:
        flash_images = [platformio_api.FlashImage(path=file, offset="0x0")]
    else:
        idedata = platformio_api.get_idedata(config)

        firmware_offset = "0x10000" if CORE.is_esp32 else "0x0"
        flash_images = [
            platformio_api.FlashImage(
                path=idedata.firmware_bin_path, offset=firmware_offset
            ),
            *idedata.extra_flash_images,
        ]

    mcu = "esp8266"
    if CORE.is_esp32:
        from esphome.components.esp32 import get_esp32_variant

        mcu = get_esp32_variant().lower()

    def run_esptool(baud_rate):
        cmd = [
            "esptool.py",
            "--before",
            "default_reset",
            "--after",
            "hard_reset",
            "--baud",
            str(baud_rate),
            "--port",
            port,
            "--chip",
            mcu,
            "write_flash",
            "-z",
            "--flash_size",
            "detect",
        ]
        for img in flash_images:
            cmd += [img.offset, img.path]

        if os.environ.get("ESPHOME_USE_SUBPROCESS") is None:
            import esptool

            return run_external_command(esptool.main, *cmd)  # pylint: disable=no-member

        return run_external_process(*cmd)

    rc = run_esptool(first_baudrate)
    if rc == 0 or first_baudrate == 115200:
        return rc
    # Try with 115200 baud rate, with some serial chips the faster baud rates do not work well
    _LOGGER.info(
        "Upload with baud rate %s failed. Trying again with baud rate 115200.",
        first_baudrate,
    )
    return run_esptool(115200)


def upload_using_platformio(config, port):
    from esphome import platformio_api

    upload_args = ["-t", "upload", "-t", "nobuild"]
    if port is not None:
        upload_args += ["--upload-port", port]
    return platformio_api.run_platformio_cli_run(config, CORE.verbose, *upload_args)


def check_permissions(port):
    if os.name == "posix":
        # Check if we can open selected serial port
        if not os.access(port, os.F_OK):
            raise EsphomeError(
                "The selected serial port does not exist. To resolve this issue, "
                "check that the device is connected to this computer with a USB cable and that "
                "the USB cable can be used for data and is not a power-only cable."
            )
        if not (os.access(port, os.R_OK | os.W_OK)):
            raise EsphomeError(
                "You do not have read or write permission on the selected serial port. "
                "To resolve this issue, you can add your user to the dialout group "
                f"by running the following command: sudo usermod -a -G dialout {os.getlogin()}. "
                "You will need to log out & back in or reboot to activate the new group access."
            )


def upload_program(args, config, host):
    check_permissions(host)
    if CORE.target_platform in (PLATFORM_ESP32, PLATFORM_ESP8266):
        file = getattr(args, "file", None)
        return upload_using_esptool(config, host, file)

    if CORE.target_platform in (PLATFORM_RP2040):
        return upload_using_platformio(config, args.device)

    if CORE.target_platform in (PLATFORM_BK72XX, PLATFORM_RTL87XX):
        return upload_using_platformio(config, host)

    return 1  # Unknown target platform


def command_upload(args, config):
    ports = {}
    while 1:
        click.clear()
        click.echo(
            color(Fore.BOLD_WHITE, "EspHome Multi Device Uploader  ")
            + f"Version: {const.__version__}"
        )
        click.echo(
            color(Fore.BOLD_WHITE, "YAML configuration file: ") + args.configuration
        )
        click.echo()

        ports = choose_ports(ports, purpose="uploading")
        if ports is None:
            return 0

        for i in ports:
            ports[i][1] = upload_program(args, config, ports[i][0])
        click.echo("\n \a - = All uploads are done = -")
        click.pause()


def command_version(args):
    safe_print(f"Version: {const.__version__}")
    return 0


def parse_args(argv):
    options_parser = argparse.ArgumentParser(add_help=False)
    options_parser.add_argument(
        "-v", "--verbose", help="Enable verbose ESPHome logs.", action="store_true"
    )
    options_parser.add_argument(
        "-q", "--quiet", help="Disable all ESPHome logs.", action="store_true"
    )
    options_parser.add_argument(
        "-s",
        "--substitution",
        nargs=2,
        action="append",
        help="Add a substitution",
        metavar=("key", "value"),
    )
    options_parser.add_argument(
        "--version",
        action="version",
        version=f"Version: {const.__version__}",
        help="Print the ESPHome version and exit.",
    )

    parser = argparse.ArgumentParser(
        description=f"ESPHome multi upload {const.__version__}",
        parents=[options_parser],
    )

    parser.add_argument(
        "configuration",
        help="Your YAML configuration file.",
    )

    parser.add_argument(
        "--file",
        help="Manually specify the binary file to upload.",
    )

    # Keep backward compatibility with the old command line format of
    # esphome <config> <command>.
    #
    # Unfortunately this can't be done by adding another configuration argument to the
    # main config parser, as argparse is greedy when parsing arguments, so in regular
    # usage it'll eat the command as the configuration argument and error out out
    # because it can't parse the configuration as a command.
    #
    # Instead, if parsing using the current format fails, construct an ad-hoc parser
    # that doesn't actually process the arguments, but parses them enough to let us
    # figure out if the old format is used. In that case, swap the command and
    # configuration in the arguments and retry with the normal parser (and raise
    # a deprecation warning).
    arguments = argv[1:]

    argcomplete.autocomplete(parser)
    return parser.parse_args(arguments)


def run_esphome(argv):
    args = parse_args(argv)
    print(args)
    setup_log(args.verbose, args.quiet, False)
    if "version" in args:
        try:
            return command_version(args)
        except EsphomeError as e:
            _LOGGER.error(e, exc_info=args.verbose)
            return 1

    _LOGGER.info("ESPHome %s", const.__version__)

    if any(os.path.basename(args.configuration) == x for x in SECRETS_FILES):
        _LOGGER.warning("Skipping secrets file %s", args.configuration)
        return 1

    CORE.config_path = args.configuration
    CORE.dashboard = False

    config = read_config(dict(args.substitution) if args.substitution else {})
    if config is None:
        return 2
    CORE.config = config

    try:
        return command_upload(args, config)
    except EsphomeError as e:
        _LOGGER.error(e, exc_info=args.verbose)
        return 1


def main():
    try:
        return run_esphome(sys.argv)
    except EsphomeError as e:
        _LOGGER.error(e)
        return 1
    except KeyboardInterrupt:
        return 1


if __name__ == "__main__":
    sys.exit(main())
