# PYTHON_ARGCOMPLETE_OK
import argparse
from datetime import datetime
import functools
import getpass
import importlib
import logging
import os
from pathlib import Path
import re
import sys
import time
from typing import Protocol

import argcomplete

# Note: Do not import modules from esphome.components here, as this would
# cause them to be loaded before external components are processed, resulting
# in the built-in version being used instead of the external component one.
from esphome import const, writer, yaml_util
import esphome.codegen as cg
from esphome.config import iter_component_configs, read_config, strip_default_ids
from esphome.const import (
    ALLOWED_NAME_CHARS,
    CONF_API,
    CONF_BAUD_RATE,
    CONF_BROKER,
    CONF_DEASSERT_RTS_DTR,
    CONF_DISABLED,
    CONF_ESPHOME,
    CONF_LEVEL,
    CONF_LOG_TOPIC,
    CONF_LOGGER,
    CONF_MDNS,
    CONF_MQTT,
    CONF_NAME,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PLATFORMIO_OPTIONS,
    CONF_PORT,
    CONF_SUBSTITUTIONS,
    CONF_TOPIC,
    ENV_NOGITIGNORE,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    SECRETS_FILES,
)
from esphome.core import CORE, EsphomeError, coroutine
from esphome.enum import StrEnum
from esphome.helpers import get_bool_env, indent, is_ip_address
from esphome.log import AnsiFore, color, setup_log
from esphome.types import ConfigType
from esphome.util import (
    get_serial_ports,
    list_yaml_files,
    run_external_command,
    run_external_process,
    safe_print,
)

_LOGGER = logging.getLogger(__name__)

# Special non-component keys that appear in configs
_NON_COMPONENT_KEYS = frozenset(
    {
        CONF_ESPHOME,
        "substitutions",
        "packages",
        "globals",
        "external_components",
        "<<",
    }
)


def detect_external_components(config: ConfigType) -> set[str]:
    """Detect external/custom components in the configuration.

    External components are those that appear in the config but are not
    part of ESPHome's built-in components and are not special config keys.

    Args:
        config: The ESPHome configuration dictionary

    Returns:
        A set of external component names
    """
    from esphome.analyze_memory.helpers import get_esphome_components

    builtin_components = get_esphome_components()
    return {
        key
        for key in config
        if key not in builtin_components and key not in _NON_COMPONENT_KEYS
    }


class ArgsProtocol(Protocol):
    device: list[str] | None
    reset: bool
    username: str | None
    password: str | None
    client_id: str | None
    topic: str | None
    file: str | None
    no_logs: bool
    only_generate: bool
    show_secrets: bool
    dashboard: bool
    configuration: str
    name: str
    upload_speed: str | None


def choose_prompt(options, purpose: str = None):
    if not options:
        raise EsphomeError(
            "Found no valid options for upload/logging, please make sure relevant "
            "sections (ota, api, mqtt, ...) are in your configuration and/or the "
            "device is plugged in."
        )

    if len(options) == 1:
        return options[0][1]

    safe_print(
        f"Found multiple options{f' for {purpose}' if purpose else ''}, please choose one:"
    )
    for i, (desc, _) in enumerate(options):
        safe_print(f"  [{i + 1}] {desc}")

    while True:
        opt = input("(number): ")
        if opt in options:
            opt = options.index(opt)
            break
        try:
            opt = int(opt)
            if opt < 1 or opt > len(options):
                raise ValueError
            break
        except ValueError:
            safe_print(color(AnsiFore.RED, f"Invalid option: '{opt}'"))
    return options[opt - 1][1]


class Purpose(StrEnum):
    UPLOADING = "uploading"
    LOGGING = "logging"


class PortType(StrEnum):
    SERIAL = "SERIAL"
    NETWORK = "NETWORK"
    MQTT = "MQTT"
    MQTTIP = "MQTTIP"


# Magic MQTT port types that require special handling
_MQTT_PORT_TYPES = frozenset({PortType.MQTT, PortType.MQTTIP})


def _resolve_with_cache(address: str, purpose: Purpose) -> list[str]:
    """Resolve an address using cache if available, otherwise return the address itself."""
    if CORE.address_cache and (cached := CORE.address_cache.get_addresses(address)):
        _LOGGER.debug("Using cached addresses for %s: %s", purpose.value, cached)
        return cached
    return [address]


def choose_upload_log_host(
    default: list[str] | str | None,
    check_default: str | None,
    purpose: Purpose,
) -> list[str]:
    # Convert to list for uniform handling
    defaults = [default] if isinstance(default, str) else default or []

    # If devices specified, resolve them
    if defaults:
        resolved: list[str] = []
        for device in defaults:
            if device == "SERIAL":
                serial_ports = get_serial_ports()
                if not serial_ports:
                    _LOGGER.warning("No serial ports found, skipping SERIAL device")
                    continue
                options = [
                    (f"{port.path} ({port.description})", port.path)
                    for port in serial_ports
                ]
                resolved.append(choose_prompt(options, purpose=purpose))
            elif device == "OTA":
                # ensure IP adresses are used first
                if is_ip_address(CORE.address) and (
                    (purpose == Purpose.LOGGING and has_api())
                    or (purpose == Purpose.UPLOADING and has_ota())
                ):
                    resolved.extend(_resolve_with_cache(CORE.address, purpose))

                if purpose == Purpose.LOGGING:
                    if has_api() and has_mqtt_ip_lookup():
                        resolved.append("MQTTIP")

                    if has_mqtt_logging():
                        resolved.append("MQTT")

                    if has_api() and has_non_ip_address() and has_resolvable_address():
                        resolved.extend(_resolve_with_cache(CORE.address, purpose))

                elif purpose == Purpose.UPLOADING:
                    if has_ota() and has_mqtt_ip_lookup():
                        resolved.append("MQTTIP")

                    if has_ota() and has_non_ip_address() and has_resolvable_address():
                        resolved.extend(_resolve_with_cache(CORE.address, purpose))
            else:
                resolved.append(device)
        if not resolved:
            raise EsphomeError(
                f"All specified devices {defaults} could not be resolved. Is the device connected to the network?"
            )
        return resolved

    # No devices specified, show interactive chooser
    options = [
        (f"{port.path} ({port.description})", port.path) for port in get_serial_ports()
    ]

    if purpose == Purpose.LOGGING:
        if has_mqtt_logging():
            mqtt_config = CORE.config[CONF_MQTT]
            options.append((f"MQTT ({mqtt_config[CONF_BROKER]})", "MQTT"))

        if has_api():
            if has_resolvable_address():
                options.append((f"Over The Air ({CORE.address})", CORE.address))
            if has_mqtt_ip_lookup():
                options.append(("Over The Air (MQTT IP lookup)", "MQTTIP"))

    elif purpose == Purpose.UPLOADING and has_ota():
        if has_resolvable_address():
            options.append((f"Over The Air ({CORE.address})", CORE.address))
        if has_mqtt_ip_lookup():
            options.append(("Over The Air (MQTT IP lookup)", "MQTTIP"))

    if check_default is not None and check_default in [opt[1] for opt in options]:
        return [check_default]
    return [choose_prompt(options, purpose=purpose)]


def has_mqtt_logging() -> bool:
    """Check if MQTT logging is available."""
    if CONF_MQTT not in CORE.config:
        return False

    mqtt_config = CORE.config[CONF_MQTT]

    # enabled by default
    if CONF_LOG_TOPIC not in mqtt_config:
        return True

    log_topic = mqtt_config[CONF_LOG_TOPIC]
    if log_topic is None:
        return False

    if CONF_TOPIC not in log_topic:
        return False

    return log_topic.get(CONF_LEVEL, None) != "NONE"


def has_mqtt() -> bool:
    """Check if MQTT is available."""
    return CONF_MQTT in CORE.config


def has_api() -> bool:
    """Check if API is available."""
    return CONF_API in CORE.config


def has_ota() -> bool:
    """Check if OTA is available."""
    return CONF_OTA in CORE.config


def has_mqtt_ip_lookup() -> bool:
    """Check if MQTT is available and IP lookup is supported."""
    from esphome.components.mqtt import CONF_DISCOVER_IP

    if CONF_MQTT not in CORE.config:
        return False
    # Default Enabled
    if CONF_DISCOVER_IP not in CORE.config[CONF_MQTT]:
        return True
    return CORE.config[CONF_MQTT][CONF_DISCOVER_IP]


def has_mdns() -> bool:
    """Check if MDNS is available."""
    return CONF_MDNS not in CORE.config or not CORE.config[CONF_MDNS][CONF_DISABLED]


def has_non_ip_address() -> bool:
    """Check if CORE.address is set and is not an IP address."""
    return CORE.address is not None and not is_ip_address(CORE.address)


def has_ip_address() -> bool:
    """Check if CORE.address is a valid IP address."""
    return CORE.address is not None and is_ip_address(CORE.address)


def has_resolvable_address() -> bool:
    """Check if CORE.address is resolvable (via mDNS, DNS, or is an IP address)."""
    # Any address (IP, mDNS hostname, or regular DNS hostname) is resolvable
    # The resolve_ip_address() function in helpers.py handles all types via AsyncResolver
    if CORE.address is None:
        return False

    if has_ip_address():
        return True

    if has_mdns():
        return True

    # .local mDNS hostnames are only resolvable if mDNS is enabled
    return not CORE.address.endswith(".local")


def mqtt_get_ip(config: ConfigType, username: str, password: str, client_id: str):
    from esphome import mqtt

    return mqtt.get_esphome_device_ip(config, username, password, client_id)


def _resolve_network_devices(
    devices: list[str], config: ConfigType, args: ArgsProtocol
) -> list[str]:
    """Resolve device list, converting MQTT magic strings to actual IP addresses.

    This function filters the devices list to:
    - Replace MQTT/MQTTIP magic strings with actual IP addresses via MQTT lookup
    - Deduplicate addresses while preserving order
    - Only resolve MQTT once even if multiple MQTT strings are present
    - If MQTT resolution fails, log a warning and continue with other devices

    Args:
        devices: List of device identifiers (IPs, hostnames, or magic strings)
        config: ESPHome configuration
        args: Command-line arguments containing MQTT credentials

    Returns:
        List of network addresses suitable for connection attempts
    """
    network_devices: list[str] = []
    mqtt_resolved: bool = False

    for device in devices:
        port_type = get_port_type(device)
        if port_type in _MQTT_PORT_TYPES:
            # Only resolve MQTT once, even if multiple MQTT entries
            if not mqtt_resolved:
                try:
                    mqtt_ips = mqtt_get_ip(
                        config, args.username, args.password, args.client_id
                    )
                    network_devices.extend(mqtt_ips)
                except EsphomeError as err:
                    _LOGGER.warning(
                        "MQTT IP discovery failed (%s), will try other devices if available",
                        err,
                    )
                mqtt_resolved = True
        elif device not in network_devices:
            # Regular network address or IP - add if not already present
            network_devices.append(device)

    return network_devices


def get_port_type(port: str) -> PortType:
    """Determine the type of port/device identifier.

    Returns:
        PortType.SERIAL for serial ports (/dev/ttyUSB0, COM1, etc.)
        PortType.MQTT for MQTT logging
        PortType.MQTTIP for MQTT IP lookup
        PortType.NETWORK for IP addresses, hostnames, or mDNS names
    """
    if port.startswith("/") or port.startswith("COM"):
        return PortType.SERIAL
    if port == "MQTT":
        return PortType.MQTT
    if port == "MQTTIP":
        return PortType.MQTTIP
    return PortType.NETWORK


def run_miniterm(config: ConfigType, port: str, args) -> int:
    from aioesphomeapi import LogParser
    import serial

    from esphome import platformio_api

    if CONF_LOGGER not in config:
        _LOGGER.info("Logger is not enabled. Not starting UART logs.")
        return 1
    baud_rate = config["logger"][CONF_BAUD_RATE]
    if baud_rate == 0:
        _LOGGER.info("UART logging is disabled (baud_rate=0). Not starting UART logs.")
        return 1
    _LOGGER.info("Starting log output from %s with baud rate %s", port, baud_rate)

    backtrace_state = False
    ser = serial.Serial()
    ser.baudrate = baud_rate
    ser.port = port

    # We can't set to False by default since it leads to toggling and hence
    # ESP32 resets on some platforms.
    if config["logger"][CONF_DEASSERT_RTS_DTR] or args.reset:
        ser.dtr = False
        ser.rts = False

    parser = LogParser()
    tries = 0
    while tries < 5:
        try:
            with ser:
                while True:
                    try:
                        raw = ser.readline()
                    except serial.SerialException:
                        _LOGGER.error("Serial port closed!")
                        return 0
                    line = (
                        raw.replace(b"\r", b"")
                        .replace(b"\n", b"")
                        .decode("utf8", "backslashreplace")
                    )
                    time_ = datetime.now()
                    nanoseconds = time_.microsecond // 1000
                    time_str = f"[{time_.hour:02}:{time_.minute:02}:{time_.second:02}.{nanoseconds:03}]"
                    safe_print(parser.parse_line(line, time_str))

                    backtrace_state = platformio_api.process_stacktrace(
                        config, line, backtrace_state=backtrace_state
                    )
        except serial.SerialException:
            tries += 1
            time.sleep(1)
    if tries >= 5:
        _LOGGER.error("Could not connect to serial port %s", port)
        return 1

    return 0


def wrap_to_code(name, comp):
    coro = coroutine(comp.to_code)

    @functools.wraps(comp.to_code)
    async def wrapped(conf):
        cg.add(cg.LineComment(f"{name}:"))
        if comp.config_schema is not None:
            conf_str = yaml_util.dump(conf)
            conf_str = conf_str.replace("//", "")
            # remove tailing \ to avoid multi-line comment warning
            conf_str = conf_str.replace("\\\n", "\n")
            cg.add(cg.LineComment(indent(conf_str)))
        await coro(conf)

    if hasattr(coro, "priority"):
        wrapped.priority = coro.priority
    return wrapped


def write_cpp(config: ConfigType) -> int:
    if not get_bool_env(ENV_NOGITIGNORE):
        writer.write_gitignore()

    generate_cpp_contents(config)
    return write_cpp_file()


def generate_cpp_contents(config: ConfigType) -> None:
    _LOGGER.info("Generating C++ source...")

    for name, component, conf in iter_component_configs(CORE.config):
        if component.to_code is not None:
            coro = wrap_to_code(name, component)
            CORE.add_job(coro, conf)

    CORE.flush_tasks()


def write_cpp_file() -> int:
    code_s = indent(CORE.cpp_main_section)
    writer.write_cpp(code_s)

    from esphome.build_gen import platformio

    platformio.write_project()

    return 0


def compile_program(args: ArgsProtocol, config: ConfigType) -> int:
    from esphome import platformio_api

    # NOTE: "Build path:" format is parsed by script/ci_memory_impact_extract.py
    # If you change this format, update the regex in that script as well
    _LOGGER.info("Compiling app... Build path: %s", CORE.build_path)
    rc = platformio_api.run_compile(config, CORE.verbose)
    if rc != 0:
        return rc
    idedata = platformio_api.get_idedata(config)
    return 0 if idedata is not None else 1


def upload_using_esptool(
    config: ConfigType, port: str, file: str, speed: int
) -> str | int:
    from esphome import platformio_api

    first_baudrate = speed or config[CONF_ESPHOME][CONF_PLATFORMIO_OPTIONS].get(
        "upload_speed", os.getenv("ESPHOME_UPLOAD_SPEED", "460800")
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
            "esptool",
            "--before",
            "default-reset",
            "--after",
            "hard-reset",
            "--baud",
            str(baud_rate),
            "--port",
            port,
            "--chip",
            mcu,
            "write-flash",
            "-z",
            "--flash-size",
            "detect",
        ]
        for img in flash_images:
            cmd += [img.offset, str(img.path)]

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


def upload_using_platformio(config: ConfigType, port: str):
    from esphome import platformio_api

    upload_args = ["-t", "upload", "-t", "nobuild"]
    if port is not None:
        upload_args += ["--upload-port", port]
    return platformio_api.run_platformio_cli_run(config, CORE.verbose, *upload_args)


def check_permissions(port: str):
    if os.name == "posix" and get_port_type(port) == PortType.SERIAL:
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
                f"by running the following command: sudo usermod -a -G dialout {getpass.getuser()}. "
                "You will need to log out & back in or reboot to activate the new group access."
            )


def upload_program(
    config: ConfigType, args: ArgsProtocol, devices: list[str]
) -> tuple[int, str | None]:
    host = devices[0]
    try:
        module = importlib.import_module("esphome.components." + CORE.target_platform)
        if getattr(module, "upload_program")(config, args, host):
            return 0, host
    except AttributeError:
        pass

    if get_port_type(host) == PortType.SERIAL:
        check_permissions(host)

        exit_code = 1
        if CORE.target_platform in (PLATFORM_ESP32, PLATFORM_ESP8266):
            file = getattr(args, "file", None)
            exit_code = upload_using_esptool(config, host, file, args.upload_speed)
        elif CORE.target_platform == PLATFORM_RP2040 or CORE.is_libretiny:
            exit_code = upload_using_platformio(config, host)
        # else: Unknown target platform, exit_code remains 1

        return exit_code, host if exit_code == 0 else None

    ota_conf = {}
    for ota_item in config.get(CONF_OTA, []):
        if ota_item[CONF_PLATFORM] == CONF_ESPHOME:
            ota_conf = ota_item
            break

    if not ota_conf:
        raise EsphomeError(
            f"Cannot upload Over the Air as the {CONF_OTA} configuration is not present or does not include {CONF_PLATFORM}: {CONF_ESPHOME}"
        )

    from esphome import espota2

    remote_port = int(ota_conf[CONF_PORT])
    password = ota_conf.get(CONF_PASSWORD)
    if getattr(args, "file", None) is not None:
        binary = Path(args.file)
    else:
        binary = CORE.firmware_bin

    # Resolve MQTT magic strings to actual IP addresses
    network_devices = _resolve_network_devices(devices, config, args)

    return espota2.run_ota(network_devices, remote_port, password, binary)


def show_logs(config: ConfigType, args: ArgsProtocol, devices: list[str]) -> int | None:
    try:
        module = importlib.import_module("esphome.components." + CORE.target_platform)
        if getattr(module, "show_logs")(config, args, devices):
            return 0
    except AttributeError:
        pass

    if "logger" not in config:
        raise EsphomeError("Logger is not configured!")

    port = devices[0]
    port_type = get_port_type(port)

    if port_type == PortType.SERIAL:
        check_permissions(port)
        return run_miniterm(config, port, args)

    # Check if we should use API for logging
    # Resolve MQTT magic strings to actual IP addresses
    if has_api() and (
        network_devices := _resolve_network_devices(devices, config, args)
    ):
        from esphome.components.api.client import run_logs

        return run_logs(config, network_devices)

    if port_type in (PortType.NETWORK, PortType.MQTT) and has_mqtt_logging():
        from esphome import mqtt

        return mqtt.show_logs(
            config, args.topic, args.username, args.password, args.client_id
        )

    raise EsphomeError("No remote or local logging method configured (api/mqtt/logger)")


def clean_mqtt(config: ConfigType, args: ArgsProtocol) -> int | None:
    from esphome import mqtt

    return mqtt.clear_topic(
        config, args.topic, args.username, args.password, args.client_id
    )


def command_wizard(args: ArgsProtocol) -> int | None:
    from esphome import wizard

    return wizard.wizard(Path(args.configuration))


def command_config(args: ArgsProtocol, config: ConfigType) -> int | None:
    if not CORE.verbose:
        config = strip_default_ids(config)
    output = yaml_util.dump(config, args.show_secrets)
    # add the console decoration so the front-end can hide the secrets
    if not args.show_secrets:
        output = re.sub(
            r"(password|key|psk|ssid)\: (.+)", r"\1: \\033[5m\2\\033[6m", output
        )
    if not CORE.quiet:
        safe_print(output)
    _LOGGER.info("Configuration is valid!")
    return 0


def command_vscode(args: ArgsProtocol) -> int | None:
    from esphome import vscode

    logging.disable(logging.INFO)
    logging.disable(logging.WARNING)
    vscode.read_config(args)


def command_compile(args: ArgsProtocol, config: ConfigType) -> int | None:
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    if args.only_generate:
        _LOGGER.info("Successfully generated source code.")
        return 0
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info("Successfully compiled program.")
    return 0


def command_upload(args: ArgsProtocol, config: ConfigType) -> int | None:
    # Get devices, resolving special identifiers like OTA
    devices = choose_upload_log_host(
        default=args.device,
        check_default=None,
        purpose=Purpose.UPLOADING,
    )

    exit_code, _ = upload_program(config, args, devices)
    if exit_code == 0:
        _LOGGER.info("Successfully uploaded program.")
    else:
        _LOGGER.warning("Failed to upload to %s", devices)
    return exit_code


def command_discover(args: ArgsProtocol, config: ConfigType) -> int | None:
    if "mqtt" in config:
        from esphome import mqtt

        return mqtt.show_discover(config, args.username, args.password, args.client_id)

    raise EsphomeError("No discover method configured (mqtt)")


def command_logs(args: ArgsProtocol, config: ConfigType) -> int | None:
    # Get devices, resolving special identifiers like OTA
    devices = choose_upload_log_host(
        default=args.device,
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    return show_logs(config, args, devices)


def command_run(args: ArgsProtocol, config: ConfigType) -> int | None:
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info("Successfully compiled program.")
    if CORE.is_host:
        from esphome.platformio_api import get_idedata

        idedata = get_idedata(config)
        if idedata is None:
            return 1
        program_path = idedata.raw["prog_path"]
        return run_external_process(program_path)

    # Get devices, resolving special identifiers like OTA
    devices = choose_upload_log_host(
        default=args.device,
        check_default=None,
        purpose=Purpose.UPLOADING,
    )

    exit_code, successful_device = upload_program(config, args, devices)
    if exit_code == 0:
        _LOGGER.info("Successfully uploaded program.")
    else:
        _LOGGER.warning("Failed to upload to %s", devices)
        return exit_code

    if args.no_logs:
        return 0

    # For logs, prefer the device we successfully uploaded to
    devices = choose_upload_log_host(
        default=successful_device,
        check_default=successful_device,
        purpose=Purpose.LOGGING,
    )
    return show_logs(config, args, devices)


def command_clean_mqtt(args: ArgsProtocol, config: ConfigType) -> int | None:
    return clean_mqtt(config, args)


def command_clean_all(args: ArgsProtocol) -> int | None:
    try:
        writer.clean_all(args.configuration)
    except OSError as err:
        _LOGGER.error("Error cleaning all files: %s", err)
        return 1
    _LOGGER.info("Done!")
    return 0


def command_mqtt_fingerprint(args: ArgsProtocol, config: ConfigType) -> int | None:
    from esphome import mqtt

    return mqtt.get_fingerprint(config)


def command_version(args: ArgsProtocol) -> int | None:
    safe_print(f"Version: {const.__version__}")
    return 0


def command_clean(args: ArgsProtocol, config: ConfigType) -> int | None:
    try:
        writer.clean_build()
    except OSError as err:
        _LOGGER.error("Error deleting build files: %s", err)
        return 1
    _LOGGER.info("Done!")
    return 0


def command_dashboard(args: ArgsProtocol) -> int | None:
    from esphome.dashboard import dashboard

    return dashboard.start_dashboard(args)


def command_update_all(args: ArgsProtocol) -> int | None:
    import click

    success = {}
    files = list_yaml_files(args.configuration)
    twidth = 60

    def print_bar(middle_text):
        middle_text = f" {middle_text} "
        width = len(click.unstyle(middle_text))
        half_line = "=" * ((twidth - width) // 2)
        safe_print(f"{half_line}{middle_text}{half_line}")

    for f in files:
        safe_print(f"Updating {color(AnsiFore.CYAN, str(f))}")
        safe_print("-" * twidth)
        safe_print()
        if CORE.dashboard:
            rc = run_external_process(
                "esphome", "--dashboard", "run", f, "--no-logs", "--device", "OTA"
            )
        else:
            rc = run_external_process(
                "esphome", "run", f, "--no-logs", "--device", "OTA"
            )
        if rc == 0:
            print_bar(f"[{color(AnsiFore.BOLD_GREEN, 'SUCCESS')}] {str(f)}")
            success[f] = True
        else:
            print_bar(f"[{color(AnsiFore.BOLD_RED, 'ERROR')}] {str(f)}")
            success[f] = False

        safe_print()
        safe_print()
        safe_print()

    print_bar(f"[{color(AnsiFore.BOLD_WHITE, 'SUMMARY')}]")
    failed = 0
    for f in files:
        if success[f]:
            safe_print(f"  - {str(f)}: {color(AnsiFore.GREEN, 'SUCCESS')}")
        else:
            safe_print(f"  - {str(f)}: {color(AnsiFore.BOLD_RED, 'FAILED')}")
            failed += 1
    return failed


def command_idedata(args: ArgsProtocol, config: ConfigType) -> int:
    import json

    from esphome import platformio_api

    logging.disable(logging.INFO)
    logging.disable(logging.WARNING)

    idedata = platformio_api.get_idedata(config)
    if idedata is None:
        return 1

    print(json.dumps(idedata.raw, indent=2) + "\n")
    return 0


def command_analyze_memory(args: ArgsProtocol, config: ConfigType) -> int:
    """Analyze memory usage by component.

    This command compiles the configuration and performs memory analysis.
    Compilation is fast if sources haven't changed (just relinking).
    """
    from esphome import platformio_api
    from esphome.analyze_memory.cli import MemoryAnalyzerCLI
    from esphome.analyze_memory.ram_strings import RamStringsAnalyzer

    # Always compile to ensure fresh data (fast if no changes - just relinks)
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info("Successfully compiled program.")

    # Get idedata for analysis
    idedata = platformio_api.get_idedata(config)
    if idedata is None:
        _LOGGER.error("Failed to get IDE data for memory analysis")
        return 1

    firmware_elf = Path(idedata.firmware_elf_path)

    # Extract external components from config
    external_components = detect_external_components(config)
    _LOGGER.debug("Detected external components: %s", external_components)

    # Perform component memory analysis
    _LOGGER.info("Analyzing memory usage...")
    analyzer = MemoryAnalyzerCLI(
        str(firmware_elf),
        idedata.objdump_path,
        idedata.readelf_path,
        external_components,
    )
    analyzer.analyze()

    # Generate and display component report
    report = analyzer.generate_report()
    print()
    print(report)

    # Perform RAM strings analysis
    _LOGGER.info("Analyzing RAM strings...")
    try:
        ram_analyzer = RamStringsAnalyzer(
            str(firmware_elf),
            objdump_path=idedata.objdump_path,
            platform=CORE.target_platform,
        )
        ram_analyzer.analyze()

        # Generate and display RAM strings report
        ram_report = ram_analyzer.generate_report()
        print()
        print(ram_report)
    except Exception as e:  # pylint: disable=broad-except
        _LOGGER.warning("RAM strings analysis failed: %s", e)

    return 0


def command_rename(args: ArgsProtocol, config: ConfigType) -> int | None:
    new_name = args.name
    for c in new_name:
        if c not in ALLOWED_NAME_CHARS:
            print(
                color(
                    AnsiFore.BOLD_RED,
                    f"'{c}' is an invalid character for names. Valid characters are: "
                    f"{ALLOWED_NAME_CHARS} (lowercase, no spaces)",
                )
            )
            return 1
    # Load existing yaml file
    raw_contents = CORE.config_path.read_text(encoding="utf-8")

    yaml = yaml_util.load_yaml(CORE.config_path)
    if CONF_ESPHOME not in yaml or CONF_NAME not in yaml[CONF_ESPHOME]:
        print(
            color(
                AnsiFore.BOLD_RED, "Complex YAML files cannot be automatically renamed."
            )
        )
        return 1
    old_name = yaml[CONF_ESPHOME][CONF_NAME]
    match = re.match(r"^\$\{?([a-zA-Z0-9_]+)\}?$", old_name)
    if match is None:
        new_raw = re.sub(
            rf"name:\s+[\"']?{old_name}[\"']?",
            f'name: "{new_name}"',
            raw_contents,
        )
    else:
        old_name = yaml[CONF_SUBSTITUTIONS][match.group(1)]
        if (
            len(
                re.findall(
                    rf"^\s+{match.group(1)}:\s+[\"']?{old_name}[\"']?",
                    raw_contents,
                    flags=re.MULTILINE,
                )
            )
            > 1
        ):
            print(color(AnsiFore.BOLD_RED, "Too many matches in YAML to safely rename"))
            return 1

        new_raw = re.sub(
            rf"^(\s+{match.group(1)}):\s+[\"']?{old_name}[\"']?",
            f'\\1: "{new_name}"',
            raw_contents,
            flags=re.MULTILINE,
        )

    new_path: Path = CORE.config_dir / (new_name + ".yaml")
    print(
        f"Updating {color(AnsiFore.CYAN, str(CORE.config_path))} to {color(AnsiFore.CYAN, str(new_path))}"
    )
    print()

    new_path.write_text(new_raw, encoding="utf-8")

    rc = run_external_process("esphome", "config", str(new_path))
    if rc != 0:
        print(color(AnsiFore.BOLD_RED, "Rename failed. Reverting changes."))
        new_path.unlink()
        return 1

    cli_args = [
        "run",
        str(new_path),
        "--no-logs",
        "--device",
        CORE.address,
    ]

    if args.dashboard:
        cli_args.insert(0, "--dashboard")

    try:
        rc = run_external_process("esphome", *cli_args)
    except KeyboardInterrupt:
        rc = 1
    if rc != 0:
        new_path.unlink()
        return 1

    if CORE.config_path != new_path:
        CORE.config_path.unlink()

    print(color(AnsiFore.BOLD_GREEN, "SUCCESS"))
    print()
    return 0


PRE_CONFIG_ACTIONS = {
    "wizard": command_wizard,
    "version": command_version,
    "dashboard": command_dashboard,
    "vscode": command_vscode,
    "update-all": command_update_all,
    "clean-all": command_clean_all,
}

POST_CONFIG_ACTIONS = {
    "config": command_config,
    "compile": command_compile,
    "upload": command_upload,
    "logs": command_logs,
    "run": command_run,
    "clean": command_clean,
    "clean-mqtt": command_clean_mqtt,
    "mqtt-fingerprint": command_mqtt_fingerprint,
    "idedata": command_idedata,
    "rename": command_rename,
    "discover": command_discover,
    "analyze-memory": command_analyze_memory,
}

SIMPLE_CONFIG_ACTIONS = [
    "clean",
    "clean-mqtt",
    "config",
]


def parse_args(argv):
    options_parser = argparse.ArgumentParser(add_help=False)
    options_parser.add_argument(
        "-v",
        "--verbose",
        help="Enable verbose ESPHome logs.",
        action="store_true",
        default=get_bool_env("ESPHOME_VERBOSE"),
    )
    options_parser.add_argument(
        "-q", "--quiet", help="Disable all ESPHome logs.", action="store_true"
    )
    options_parser.add_argument(
        "-l",
        "--log-level",
        help="Set the log level.",
        default=os.getenv("ESPHOME_LOG_LEVEL", "INFO"),
        action="store",
        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
    )
    options_parser.add_argument(
        "--dashboard", help=argparse.SUPPRESS, action="store_true"
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
        "--mdns-address-cache",
        help="mDNS address cache mapping in format 'hostname=ip1,ip2'",
        action="append",
        default=[],
    )
    options_parser.add_argument(
        "--dns-address-cache",
        help="DNS address cache mapping in format 'hostname=ip1,ip2'",
        action="append",
        default=[],
    )
    options_parser.add_argument(
        "--testing-mode",
        help="Enable testing mode (disables validation checks for grouped component testing)",
        action="store_true",
        default=False,
    )

    parser = argparse.ArgumentParser(
        description=f"ESPHome {const.__version__}", parents=[options_parser]
    )

    parser.add_argument(
        "--version",
        action="version",
        version=f"Version: {const.__version__}",
        help="Print the ESPHome version and exit.",
    )

    mqtt_options = argparse.ArgumentParser(add_help=False)
    mqtt_options.add_argument("--topic", help="Manually set the MQTT topic.")
    mqtt_options.add_argument("--username", help="Manually set the MQTT username.")
    mqtt_options.add_argument("--password", help="Manually set the MQTT password.")
    mqtt_options.add_argument("--client-id", help="Manually set the MQTT client id.")

    subparsers = parser.add_subparsers(
        help="Command to run:", dest="command", metavar="command"
    )
    subparsers.required = True

    parser_config = subparsers.add_parser(
        "config", help="Validate the configuration and spit it out."
    )
    parser_config.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )
    parser_config.add_argument(
        "--show-secrets", help="Show secrets in output.", action="store_true"
    )

    parser_compile = subparsers.add_parser(
        "compile", help="Read the configuration and compile a program."
    )
    parser_compile.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )
    parser_compile.add_argument(
        "--only-generate",
        help="Only generate source code, do not compile.",
        action="store_true",
    )

    parser_upload = subparsers.add_parser(
        "upload",
        help="Validate the configuration and upload the latest binary.",
        parents=[mqtt_options],
    )
    parser_upload.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )
    parser_upload.add_argument(
        "--device",
        action="append",
        help="Manually specify the serial port/address to use, for example /dev/ttyUSB0. Can be specified multiple times for fallback addresses.",
    )
    parser_upload.add_argument(
        "--upload_speed",
        help="Override the default or configured upload speed.",
    )
    parser_upload.add_argument(
        "--file",
        help="Manually specify the binary file to upload.",
    )

    parser_logs = subparsers.add_parser(
        "logs",
        help="Validate the configuration and show all logs.",
        aliases=["log"],
        parents=[mqtt_options],
    )
    parser_logs.add_argument(
        "configuration", help="Your YAML configuration file.", nargs=1
    )
    parser_logs.add_argument(
        "--device",
        action="append",
        help="Manually specify the serial port/address to use, for example /dev/ttyUSB0. Can be specified multiple times for fallback addresses.",
    )
    parser_logs.add_argument(
        "--reset",
        "-r",
        action="store_true",
        help="Reset the device before starting serial logs.",
        default=os.getenv("ESPHOME_SERIAL_LOGGING_RESET"),
    )

    parser_discover = subparsers.add_parser(
        "discover",
        help="Validate the configuration and show all discovered devices.",
        parents=[mqtt_options],
    )
    parser_discover.add_argument(
        "configuration", help="Your YAML configuration file.", nargs=1
    )

    parser_run = subparsers.add_parser(
        "run",
        help="Validate the configuration, create a binary, upload it, and start logs.",
        parents=[mqtt_options],
    )
    parser_run.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )
    parser_run.add_argument(
        "--device",
        action="append",
        help="Manually specify the serial port/address to use, for example /dev/ttyUSB0. Can be specified multiple times for fallback addresses.",
    )
    parser_run.add_argument(
        "--upload_speed",
        help="Override the default or configured upload speed.",
    )
    parser_run.add_argument(
        "--no-logs", help="Disable starting logs.", action="store_true"
    )
    parser_run.add_argument(
        "--reset",
        "-r",
        action="store_true",
        help="Reset the device before starting serial logs.",
        default=os.getenv("ESPHOME_SERIAL_LOGGING_RESET"),
    )

    parser_clean = subparsers.add_parser(
        "clean-mqtt",
        help="Helper to clear retained messages from an MQTT topic.",
        parents=[mqtt_options],
    )
    parser_clean.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )

    parser_wizard = subparsers.add_parser(
        "wizard",
        help="A helpful setup wizard that will guide you through setting up ESPHome.",
    )
    parser_wizard.add_argument("configuration", help="Your YAML configuration file.")

    parser_fingerprint = subparsers.add_parser(
        "mqtt-fingerprint", help="Get the SSL fingerprint from a MQTT broker."
    )
    parser_fingerprint.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )

    subparsers.add_parser("version", help="Print the ESPHome version and exit.")

    parser_clean = subparsers.add_parser(
        "clean", help="Delete all temporary build files."
    )
    parser_clean.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )

    parser_clean_all = subparsers.add_parser(
        "clean-all", help="Clean all build and platform files."
    )
    parser_clean_all.add_argument(
        "configuration", help="Your YAML file or configuration directory.", nargs="*"
    )

    parser_dashboard = subparsers.add_parser(
        "dashboard", help="Create a simple web server for a dashboard."
    )
    parser_dashboard.add_argument(
        "configuration", help="Your YAML configuration file directory."
    )
    parser_dashboard.add_argument(
        "--port",
        help="The HTTP port to open connections on. Defaults to 6052.",
        type=int,
        default=6052,
    )
    parser_dashboard.add_argument(
        "--address",
        help="The address to bind to.",
        type=str,
        default="0.0.0.0",
    )
    parser_dashboard.add_argument(
        "--username",
        help="The optional username to require for authentication.",
        type=str,
        default="",
    )
    parser_dashboard.add_argument(
        "--password",
        help="The optional password to require for authentication.",
        type=str,
        default="",
    )
    parser_dashboard.add_argument(
        "--open-ui", help="Open the dashboard UI in a browser.", action="store_true"
    )
    parser_dashboard.add_argument(
        "--ha-addon", help=argparse.SUPPRESS, action="store_true"
    )
    parser_dashboard.add_argument(
        "--socket", help="Make the dashboard serve under a unix socket", type=str
    )

    parser_vscode = subparsers.add_parser("vscode")
    parser_vscode.add_argument("configuration", help="Your YAML configuration file.")
    parser_vscode.add_argument("--ace", action="store_true")

    parser_update = subparsers.add_parser("update-all")
    parser_update.add_argument(
        "configuration", help="Your YAML configuration file or directory.", nargs="+"
    )

    parser_idedata = subparsers.add_parser("idedata")
    parser_idedata.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs=1
    )

    parser_rename = subparsers.add_parser(
        "rename",
        help="Rename a device in YAML, compile the binary and upload it.",
    )
    parser_rename.add_argument(
        "configuration", help="Your YAML configuration file.", nargs=1
    )
    parser_rename.add_argument("name", help="The new name for the device.", type=str)

    parser_analyze_memory = subparsers.add_parser(
        "analyze-memory",
        help="Analyze memory usage by component.",
    )
    parser_analyze_memory.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
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

    if len(arguments) > 0 and arguments[0] in SIMPLE_CONFIG_ACTIONS:
        args, unknown_args = parser.parse_known_args(arguments)
        if unknown_args:
            _LOGGER.warning("Ignored unrecognized arguments: %s", unknown_args)
        return args

    return parser.parse_args(arguments)


def run_esphome(argv):
    from esphome.address_cache import AddressCache

    args = parse_args(argv)
    CORE.dashboard = args.dashboard
    CORE.testing_mode = args.testing_mode

    # Create address cache from command-line arguments
    CORE.address_cache = AddressCache.from_cli_args(
        args.mdns_address_cache, args.dns_address_cache
    )
    # Override log level if verbose is set
    if args.verbose:
        args.log_level = "DEBUG"
    elif args.quiet:
        args.log_level = "CRITICAL"

    setup_log(
        log_level=args.log_level,
        # Show timestamp for dashboard access logs
        include_timestamp=args.command == "dashboard",
    )

    if args.command in PRE_CONFIG_ACTIONS:
        try:
            return PRE_CONFIG_ACTIONS[args.command](args)
        except EsphomeError as e:
            _LOGGER.error(e, exc_info=args.verbose)
            return 1

    _LOGGER.info("ESPHome %s", const.__version__)

    for conf_path in args.configuration:
        conf_path = Path(conf_path)
        if any(conf_path.name == x for x in SECRETS_FILES):
            _LOGGER.warning("Skipping secrets file %s", conf_path)
            continue

        CORE.config_path = conf_path
        CORE.dashboard = args.dashboard

        # For logs command, skip updating external components
        skip_external = args.command == "logs"
        config = read_config(
            dict(args.substitution) if args.substitution else {},
            skip_external_update=skip_external,
        )
        if config is None:
            return 2
        CORE.config = config

        if args.command not in POST_CONFIG_ACTIONS:
            safe_print(f"Unknown command {args.command}")

        try:
            rc = POST_CONFIG_ACTIONS[args.command](args, config)
        except EsphomeError as e:
            _LOGGER.error(e, exc_info=args.verbose)
            return 1
        if rc != 0:
            return rc

        CORE.reset()
    return 0


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
