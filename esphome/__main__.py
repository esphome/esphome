# PYTHON_ARGCOMPLETE_OK
import argparse
from collections.abc import Callable
from contextlib import suppress
from datetime import datetime
import functools
import getpass
import importlib
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from typing import Protocol

import argcomplete

# Note: Do not import modules from esphome.components here, as this would
# cause them to be loaded before external components are processed, resulting
# in the built-in version being used instead of the external component one.
from esphome import const
import esphome.codegen as cg
from esphome.config import iter_component_configs, read_config, strip_default_ids
from esphome.const import (
    ALLOWED_NAME_CHARS,
    ARGUMENT_HELP_DEVICE,
    CONF_API,
    CONF_AUTH,
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
    CONF_NAME_ADD_MAC_SUFFIX,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PLATFORMIO_OPTIONS,
    CONF_PORT,
    CONF_SUBSTITUTIONS,
    CONF_TOPIC,
    CONF_USERNAME,
    CONF_WEB_SERVER,
    ENV_NOGITIGNORE,
    KEY_CORE,
    KEY_NATIVE_IDF,
    KEY_TARGET_PLATFORM,
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
    PICOTOOL_PACKAGE,
    FlashImage,
    detect_rp2040_bootsel,
    get_picotool_path,
    get_serial_ports,
    is_picotool_usb_permission_error,
    list_yaml_files,
    run_external_command,
    run_external_process,
    safe_print,
)

# Keep expensive imports (zeroconf, writer, yaml_util, etc.) out of this
# module's top level. Every `esphome` invocation — including fast paths
# like `esphome version` — pays the cost of what's imported here before
# any command runs. Import inside the function that needs it instead.
# `script/check_import_time.py` enforces a budget in CI.

_LOGGER = logging.getLogger(__name__)

ESPHOME_COMMAND = [sys.executable, "-m", "esphome"]

# Maximum buffer size for serial log reading to prevent unbounded memory growth
SERIAL_BUFFER_MAX_SIZE = 65536

_RP2040_BOOTSEL_INSTRUCTIONS = (
    "To enter BOOTSEL mode:\n"
    "  1. Unplug the device\n"
    "  2. Hold the BOOT/BOOTSEL button\n"
    "  3. Plug in the USB cable while holding the button\n"
    "  4. Release the button - the device should appear as a USB drive (RPI-RP2)\n"
    "Then run the upload command again."
)

_RP2040_UDEV_HINT = (
    "You may need to add a udev rule for RP2040 devices. "
    "See: https://github.com/raspberrypi/picotool"
    "/blob/master/udev/60-picotool.rules"
)

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
    native_idf: bool


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
    BOOTSEL = "BOOTSEL"


# Magic MQTT port types that require special handling
_MQTT_PORT_TYPES = frozenset({PortType.MQTT, PortType.MQTTIP})


def _resolve_with_cache(address: str, purpose: Purpose) -> list[str]:
    """Resolve an address using cache if available, otherwise return the address itself."""
    if CORE.address_cache and (cached := CORE.address_cache.get_addresses(address)):
        _LOGGER.debug("Using cached addresses for %s: %s", purpose.value, cached)
        return cached
    return [address]


def _populate_mdns_cache(hosts_to_addresses: dict[str, list[str]]) -> None:
    """Store discovered ``host -> [ips]`` entries in ``CORE.address_cache``.

    Ensures ``CORE.address_cache`` exists, then records each mDNS hostname so
    the downstream resolution path (``resolve_ip_address``) can skip opening a
    second Zeroconf client.
    """
    from esphome.address_cache import AddressCache

    if CORE.address_cache is None:
        CORE.address_cache = AddressCache()
    for host, addresses in hosts_to_addresses.items():
        if addresses:
            _LOGGER.debug("Caching mDNS result %s -> %s", host, addresses)
        CORE.address_cache.add_mdns_addresses(host, addresses)


def _discover_mac_suffix_devices() -> list[str] | None:
    """Discover ``<name>-<mac>.local`` devices and cache their IPs.

    Returns:
        - ``None`` when discovery isn't applicable (``name_add_mac_suffix`` off,
          mDNS disabled, or ``CORE.address`` is already an IP). Callers should
          then fall back to whatever default OTA address they normally use.
        - ``[]`` when discovery ran but found nothing. Callers should NOT fall
          back to the base name: with ``name_add_mac_suffix`` enabled, the base
          name by definition doesn't exist on the network.
        - A non-empty sorted list of ``.local`` hostnames on success.

    Populates ``CORE.address_cache`` so downstream resolution (``espota2`` or
    ``aioesphomeapi`` via :func:`_resolve_network_devices`) reuses the IPs we
    already have without opening a second Zeroconf client.
    """
    if not (has_name_add_mac_suffix() and has_mdns() and has_non_ip_address()):
        return None
    from esphome.zeroconf import discover_mdns_devices

    _LOGGER.info("Discovering devices...")
    if not (discovered := discover_mdns_devices(CORE.name)):
        _LOGGER.warning(
            "No devices matching '%s-<mac>.local' were discovered.", CORE.name
        )
        return []
    _populate_mdns_cache(discovered)
    return list(discovered)


def _ota_hostnames_for_default(purpose: Purpose) -> list[str]:
    """Return OTA hostname(s) for the ``--device OTA`` / default-resolve path.

    When ``name_add_mac_suffix`` is enabled, returns discovered
    ``<name>-<mac>.local`` hostnames (possibly empty — in which case the
    caller should not fall back to the base name). Otherwise falls back to
    the cache-resolved ``CORE.address``.
    """
    if (discovered := _discover_mac_suffix_devices()) is not None:
        return discovered
    return _resolve_with_cache(CORE.address, purpose)


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
                        resolved.extend(_ota_hostnames_for_default(purpose))

                elif purpose == Purpose.UPLOADING:
                    if has_ota() and has_mqtt_ip_lookup():
                        resolved.append("MQTTIP")

                    if has_ota() and has_non_ip_address() and has_resolvable_address():
                        resolved.extend(_ota_hostnames_for_default(purpose))
            else:
                resolved.append(device)
        if not resolved:
            if CORE.dashboard:
                hint = "If you know the IP, set 'use_address' in your network config."
            else:
                hint = "If you know the IP, try --device <IP>"
            raise EsphomeError(
                f"All specified devices {defaults} could not be resolved. "
                f"Is the device connected to the network? {hint}"
            )
        return resolved

    # No devices specified, show interactive chooser
    options = [
        (f"{port.path} ({port.description})", port.path) for port in get_serial_ports()
    ]

    # Add RP2040 BOOTSEL device option when uploading
    bootsel_permission_error = False
    if (
        purpose == Purpose.UPLOADING
        and CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) == PLATFORM_RP2040
        and (picotool := _find_picotool()) is not None
    ):
        bootsel = detect_rp2040_bootsel(picotool)
        if bootsel.device_count > 0:
            options.append(("RP2040 BOOTSEL (via picotool)", "BOOTSEL"))
        elif bootsel.permission_error:
            bootsel_permission_error = True

    # Annotate the OTA chooser entry only in the non-default case: when the
    # config has web_server OTA but no native API OTA, the upload will fall
    # through to the HTTP path and the user benefits from seeing that
    # explicitly. The native-API path is the default and gets a plain label
    # to avoid noise on the most common scenario. For LOGGING the OTA
    # transport doesn't apply, so always leave the label plain.
    if purpose == Purpose.UPLOADING and not has_native_ota() and has_web_server_ota():
        ota_suffix = " via web_server"
    else:
        ota_suffix = ""

    def add_ota_options() -> None:
        """Add OTA options, using mDNS discovery if name_add_mac_suffix is enabled."""
        if (discovered := _discover_mac_suffix_devices()) is not None:
            # Discovery was applicable. Use whatever we found — on empty,
            # intentionally skip the base-name fallback since with
            # name_add_mac_suffix on, the base name doesn't exist on the net.
            for host in discovered:
                options.append((f"Over The Air{ota_suffix} ({host})", host))
        elif has_resolvable_address():
            options.append((f"Over The Air{ota_suffix} ({CORE.address})", CORE.address))
        if has_mqtt_ip_lookup():
            options.append((f"Over The Air{ota_suffix} (MQTT IP lookup)", "MQTTIP"))

    if purpose == Purpose.LOGGING:
        if has_mqtt_logging():
            mqtt_config = CORE.config[CONF_MQTT]
            options.append((f"MQTT ({mqtt_config[CONF_BROKER]})", "MQTT"))

        if has_api():
            add_ota_options()

    elif purpose == Purpose.UPLOADING and has_ota():
        add_ota_options()

    # Show helpful BOOTSEL instructions for RP2040 when no BOOTSEL device is found
    if (
        purpose == Purpose.UPLOADING
        and CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) == PLATFORM_RP2040
        and not any(get_port_type(opt[1]) == PortType.BOOTSEL for opt in options)
    ):
        if bootsel_permission_error:
            _LOGGER.warning(
                "An RP2040 device in BOOTSEL mode was detected but could "
                "not be accessed due to USB permissions."
            )
            if sys.platform.startswith("linux"):
                _LOGGER.warning(_RP2040_UDEV_HINT)
        if not options:
            raise EsphomeError(
                f"No RP2040 device found. {_RP2040_BOOTSEL_INSTRUCTIONS}"
            )
        _LOGGER.info("Tip: %s", _RP2040_BOOTSEL_INSTRUCTIONS)

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
    """Check if any network OTA upload is available.

    True if the config exposes either ``platform: esphome`` (native API
    OTA) or ``platform: web_server`` (HTTP OTA). Both reach the device
    over the same network stack, so the OTA discovery path treats them
    interchangeably; ``upload_program`` picks the actual transport based
    on ``--ota-platform`` and what's configured.
    """
    return has_native_ota() or has_web_server_ota()


def has_native_ota() -> bool:
    """Check if native API OTA upload is available (``platform: esphome``)."""
    if CONF_OTA not in CORE.config:
        return False
    return any(
        ota_item.get(CONF_PLATFORM) == CONF_ESPHOME
        for ota_item in CORE.config[CONF_OTA]
    )


def has_web_server_ota() -> bool:
    """Check if web_server OTA upload is available (``platform: web_server``)."""
    if CONF_OTA not in CORE.config:
        return False
    return any(
        ota_item.get(CONF_PLATFORM) == CONF_WEB_SERVER
        for ota_item in CORE.config[CONF_OTA]
    )


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


def has_name_add_mac_suffix() -> bool:
    """Check if name_add_mac_suffix is enabled in the config."""
    if CORE.config is None:
        return False
    esphome_config = CORE.config.get(CONF_ESPHOME, {})
    return esphome_config.get(CONF_NAME_ADD_MAC_SUFFIX, False)


def mqtt_get_ip(
    config: ConfigType, username: str, password: str, client_id: str
) -> list[str]:
    from esphome import mqtt

    return mqtt.get_esphome_device_ip(config, username, password, client_id)


def _resolve_network_devices(
    devices: list[str], config: ConfigType, args: ArgsProtocol
) -> list[str]:
    """Resolve device list, converting MQTT magic strings to actual IP addresses.

    This function filters the devices list to:
    - Replace MQTT/MQTTIP magic strings with actual IP addresses via MQTT lookup
    - Expand hostnames that are already in ``CORE.address_cache`` to their
      cached IPs so downstream code (e.g. aioesphomeapi) doesn't open a second
      Zeroconf client to resolve them
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
                    # pylint can't infer mqtt_get_ip's return through its
                    # lazy ``from esphome import mqtt`` import, so it flags
                    # the genexpr below.
                    network_devices.extend(
                        addr
                        for addr in mqtt_ips  # pylint: disable=not-an-iterable
                        if addr not in network_devices
                    )
                except EsphomeError as err:
                    _LOGGER.warning(
                        "MQTT IP discovery failed (%s), will try other devices if available",
                        err,
                    )
                mqtt_resolved = True
            continue

        # If the hostname is already in the address cache (e.g. populated by
        # mDNS discovery), substitute the cached IPs so aioesphomeapi doesn't
        # open its own Zeroconf to re-resolve it.
        if CORE.address_cache and (cached := CORE.address_cache.get_addresses(device)):
            network_devices.extend(
                addr for addr in cached if addr not in network_devices
            )
        elif device not in network_devices:
            # Regular network address or IP - add if not already present
            network_devices.append(device)

    return network_devices


def get_port_type(port: str) -> PortType:
    """Determine the type of port/device identifier.

    Returns:
        PortType.SERIAL for serial ports (/dev/ttyUSB0, COM1, etc.)
        PortType.BOOTSEL for RP2040 BOOTSEL upload via picotool
        PortType.MQTT for MQTT logging
        PortType.MQTTIP for MQTT IP lookup
        PortType.NETWORK for IP addresses, hostnames, or mDNS names
    """
    if port == "BOOTSEL":
        return PortType.BOOTSEL
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

    if CONF_LOGGER not in config:
        _LOGGER.info("Logger is not enabled. Not starting UART logs.")
        return 1
    baud_rate = config["logger"][CONF_BAUD_RATE]
    if baud_rate == 0:
        _LOGGER.info("UART logging is disabled (baud_rate=0). Not starting UART logs.")
        return 1
    _LOGGER.info("Starting log output from %s with baud rate %s", port, baud_rate)

    process_stacktrace = None

    try:
        module = importlib.import_module("esphome.components." + CORE.target_platform)
        process_stacktrace = getattr(module, "process_stacktrace")
    except (AttributeError, ImportError):
        _LOGGER.info(
            'Stacktrace analysis is unavailable: no compatible analyzer found for target platform "%s".',
            CORE.target_platform,
        )

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
                buffer = b""
                ser.timeout = 0.1  # 100ms timeout for non-blocking reads
                while True:
                    try:
                        # Read all available data and timestamp it
                        chunk = ser.read(ser.in_waiting or 1)
                        if not chunk:
                            continue
                        time_ = datetime.now()
                        milliseconds = time_.microsecond // 1000
                        time_str = f"[{time_.hour:02}:{time_.minute:02}:{time_.second:02}.{milliseconds:03}]"

                        # Add to buffer and process complete lines
                        # Limit buffer size to prevent unbounded memory growth
                        # if device sends data without newlines
                        buffer += chunk
                        if len(buffer) > SERIAL_BUFFER_MAX_SIZE:
                            buffer = buffer[-SERIAL_BUFFER_MAX_SIZE:]
                        while b"\n" in buffer:
                            raw_line, buffer = buffer.split(b"\n", 1)
                            line = raw_line.replace(b"\r", b"").decode(
                                "utf8", "backslashreplace"
                            )
                            safe_print(parser.parse_line(line, time_str))

                            if process_stacktrace is not None:
                                backtrace_state = process_stacktrace(
                                    config, line, backtrace_state
                                )
                    except serial.SerialException:
                        _LOGGER.error("Serial port closed!")
                        return 0
        except serial.SerialException:
            tries += 1
            time.sleep(1)
    if tries >= 5:
        _LOGGER.error("Could not connect to serial port %s", port)
        return 1

    return 0


def _wrap_to_code(name, comp, yaml_util):
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


def write_cpp(config: ConfigType, native_idf: bool = False) -> int:
    from esphome import writer

    if not get_bool_env(ENV_NOGITIGNORE):
        writer.write_gitignore()

    # Store native_idf flag so esp32 component can check it
    CORE.data[KEY_NATIVE_IDF] = native_idf

    generate_cpp_contents(config)
    return write_cpp_file(native_idf=native_idf)


def generate_cpp_contents(config: ConfigType) -> None:
    from esphome import yaml_util

    _LOGGER.info("Generating C++ source...")

    for name, component, conf in iter_component_configs(CORE.config):
        if component.to_code is not None:
            coro = _wrap_to_code(name, component, yaml_util)
            CORE.add_job(coro, conf)

    CORE.flush_tasks()


def write_cpp_file(native_idf: bool = False) -> int:
    from esphome import writer

    code_s = indent(CORE.cpp_main_section)
    writer.write_cpp(code_s)

    if native_idf and CORE.is_esp32 and CORE.target_framework == "esp-idf":
        from esphome.build_gen import espidf

        espidf.write_project()
    else:
        from esphome.build_gen import platformio

        platformio.write_project()

    return 0


def compile_program(args: ArgsProtocol, config: ConfigType) -> int:
    native_idf = getattr(args, "native_idf", False)

    # NOTE: "Build path:" format is parsed by script/ci_memory_impact_extract.py
    # If you change this format, update the regex in that script as well
    _LOGGER.info("Compiling app... Build path: %s", CORE.build_path)

    if native_idf and CORE.is_esp32 and CORE.target_framework == "esp-idf":
        from esphome import espidf_api

        rc = espidf_api.run_compile(config, CORE.verbose)
        if rc != 0:
            return rc

        # Create factory.bin and ota.bin
        espidf_api.create_factory_bin()
        espidf_api.create_ota_bin()
    else:
        from esphome import platformio_api

        rc = platformio_api.run_compile(config, CORE.verbose)
        if rc != 0:
            return rc

        idedata = platformio_api.get_idedata(config)
        if idedata is None:
            return 1

    # Check if firmware was rebuilt and emit build_info + create manifest
    _check_and_emit_build_info()

    return 0


def _check_and_emit_build_info() -> None:
    """Check if firmware was rebuilt and emit build_info."""
    import json

    firmware_path = CORE.firmware_bin
    build_info_json_path = CORE.relative_build_path("build_info.json")

    # Check if both files exist
    if not firmware_path.exists() or not build_info_json_path.exists():
        return

    # Check if firmware is newer than build_info (indicating a relink occurred)
    if firmware_path.stat().st_mtime <= build_info_json_path.stat().st_mtime:
        return

    # Read build_info from JSON
    try:
        with open(build_info_json_path, encoding="utf-8") as f:
            build_info = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        _LOGGER.debug("Failed to read build_info: %s", e)
        return

    config_hash = build_info.get("config_hash")
    build_time_str = build_info.get("build_time_str")

    if config_hash is None or build_time_str is None:
        return

    # Emit build_info with human-readable time
    _LOGGER.info(
        "Build Info: config_hash=0x%08x build_time_str=%s", config_hash, build_time_str
    )


def _get_configured_xtal_freq() -> int | None:
    """Read the configured crystal frequency from the sdkconfig file."""
    sdkconfig_path = CORE.relative_build_path(f"sdkconfig.{CORE.name}")
    if not sdkconfig_path.is_file():
        return None
    with suppress(OSError, ValueError):
        content = sdkconfig_path.read_text()
        for line in content.splitlines():
            if line.startswith("CONFIG_XTAL_FREQ="):
                return int(line.split("=", 1)[1])
    return None


def _make_crystal_freq_callback(
    configured_freq: int,
) -> Callable[[str], str | None]:
    """Create a callback that checks esptool crystal frequency output."""
    crystal_re = re.compile(r"Crystal frequency:\s+(\d+)\s*MHz")

    def check_crystal_line(line: str) -> str | None:
        if not (match := crystal_re.search(line)):
            return None
        detected = int(match.group(1))
        if detected == configured_freq:
            return None
        return (
            f"\n\033[33mWARNING: Crystal frequency mismatch! "
            f"Device reports {detected}MHz but firmware is configured "
            f"for {configured_freq}MHz.\n"
            f"UART logging and other clock-dependent features will not "
            f"work correctly.\n"
            f"Set the correct crystal frequency with sdkconfig_options:\n"
            f"  esp32:\n"
            f"    framework:\n"
            f"      sdkconfig_options:\n"
            f"        CONFIG_XTAL_FREQ_{detected}: 'y'\033[0m\n\n"
        )

    return check_crystal_line


def upload_using_esptool(
    config: ConfigType, port: str, file: str, speed: int
) -> str | int:
    first_baudrate = speed or config[CONF_ESPHOME][CONF_PLATFORMIO_OPTIONS].get(
        "upload_speed", os.getenv("ESPHOME_UPLOAD_SPEED", "460800")
    )

    if file is not None:
        flash_images = [FlashImage(path=file, offset="0x0")]
    else:
        from esphome import platformio_api

        idedata = platformio_api.get_idedata(config)

        firmware_offset = "0x10000" if CORE.is_esp32 else "0x0"
        flash_images = [
            FlashImage(path=idedata.firmware_bin_path, offset=firmware_offset),
        ]
        for image in idedata.extra_flash_images:
            if not image.path.is_file():
                _LOGGER.warning(
                    "Skipping missing flash image declared by platform: %s",
                    image.path,
                )
                continue
            flash_images.append(image)

    mcu = "esp8266"
    if CORE.is_esp32:
        from esphome.components.esp32 import get_esp32_variant

        mcu = get_esp32_variant().lower()

    line_callbacks: list[Callable[[str], str | None]] = []
    if (
        CORE.is_esp32
        and file is None
        and (configured_freq := _get_configured_xtal_freq()) is not None
    ):
        line_callbacks.append(_make_crystal_freq_callback(configured_freq))

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

            return run_external_command(
                esptool.main,  # pylint: disable=no-member
                *cmd,
                line_callbacks=line_callbacks,
            )

        return run_external_process(*cmd, line_callbacks=line_callbacks)

    rc = run_esptool(first_baudrate)
    if rc == 0 or first_baudrate == 115200:
        return rc
    # Try with 115200 baud rate, with some serial chips the faster baud rates do not work well
    _LOGGER.info(
        "Upload with baud rate %s failed. Trying again with baud rate 115200.",
        first_baudrate,
    )
    return run_esptool(115200)


def upload_using_platformio(config: ConfigType, port: str) -> int:
    from esphome import platformio_api

    # RP2040 platform-raspberrypi build recipe expects firmware.bin.signed for
    # the upload target, but 'nobuild' skips the build phase that creates it.
    # Create it here so the upload doesn't fail.
    if CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) == PLATFORM_RP2040:
        idedata = platformio_api.get_idedata(config)
        build_dir = Path(idedata.firmware_elf_path).parent
        firmware_bin = build_dir / "firmware.bin"
        signed_bin = build_dir / "firmware.bin.signed"
        if firmware_bin.is_file() and not signed_bin.is_file():
            shutil.copy2(firmware_bin, signed_bin)

    upload_args = ["-t", "upload", "-t", "nobuild"]
    if port is not None:
        upload_args += ["--upload-port", port]
    return platformio_api.run_platformio_cli_run(config, CORE.verbose, *upload_args)


def _find_picotool() -> Path | None:
    """Find the picotool binary from PlatformIO packages."""
    from esphome import platformio_api

    try:
        idedata = platformio_api.get_idedata(CORE.config)
    except Exception:  # noqa: BLE001  # pylint: disable=broad-except
        return None
    return get_picotool_path(idedata.cc_path)


def upload_using_picotool(config: ConfigType) -> int:
    """Upload firmware to RP2040 in BOOTSEL mode using picotool.

    Uses picotool to load the ELF firmware directly via USB, avoiding
    the mass storage copy approach that causes "disk not ejected properly"
    warnings on macOS.
    """
    from esphome import platformio_api

    idedata = platformio_api.get_idedata(config)
    firmware_elf = Path(idedata.firmware_elf_path)

    if not firmware_elf.is_file():
        _LOGGER.error(
            "Firmware ELF file not found at %s. "
            "Make sure the project has been compiled first.",
            firmware_elf,
        )
        return 1

    picotool = get_picotool_path(idedata.cc_path)
    if picotool is None:
        _LOGGER.error(
            "picotool not found. Ensure the RP2040 PlatformIO platform "
            "is installed (%s).",
            PICOTOOL_PACKAGE,
        )
        return 1

    _LOGGER.info("Uploading firmware to RP2040 via picotool...")
    try:
        # Don't capture stdout — let picotool write directly to the terminal
        # so progress bars display in real-time with \r updates.
        # Capture stderr only so we can detect permission errors.
        result = subprocess.run(
            [str(picotool), "load", "-v", "-x", str(firmware_elf)],
            stderr=subprocess.PIPE,
            timeout=60,
            check=False,
        )
    except subprocess.TimeoutExpired:
        _LOGGER.error("picotool upload timed out after 60 seconds.")
        return 1
    except OSError as err:
        _LOGGER.error("Failed to run picotool: %s", err)
        return 1

    if result.returncode != 0:
        stderr = result.stderr.decode("utf-8", errors="replace").strip()
        if stderr:
            for line in stderr.splitlines():
                safe_print(line)
        if is_picotool_usb_permission_error(stderr):
            msg = "Permission denied accessing USB device."
            if sys.platform.startswith("linux"):
                msg += f" {_RP2040_UDEV_HINT}"
            _LOGGER.error(msg)
        else:
            _LOGGER.error("picotool upload failed (exit code %d).", result.returncode)
        return 1

    return 0


def _wait_for_serial_port(
    port: str | None = None,
    timeout: float = 30.0,
    known_ports: set[str] | None = None,
) -> None:
    """Wait for a serial port to appear, e.g. after a device reboot.

    USB-CDC devices disappear briefly after flashing while the device
    reboots and re-enumerates on the USB bus.

    If port is given, wait for that specific path. If known_ports is
    given, wait for a new port that wasn't in the set. Otherwise wait
    for any serial port to appear.
    """

    def _port_found() -> bool:
        if port is not None:
            if os.name == "posix":
                return os.path.exists(port)
            return any(p.path == port for p in get_serial_ports())
        ports = get_serial_ports()
        if known_ports is not None:
            return any(p.path not in known_ports for p in ports)
        return bool(ports)

    if _port_found():
        return
    if port is not None:
        _LOGGER.info("Waiting for %s to come online...", port)
    else:
        _LOGGER.info("Waiting for device to reboot...")
    start = time.monotonic()
    while time.monotonic() - start < timeout:
        time.sleep(0.05)
        if _port_found():
            time.sleep(0.05)
            return


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

    port_type = get_port_type(host)

    # MQTT and MQTTIP are also OTA paths; MQTTIP gets resolved to a real IP later by
    # _resolve_network_devices(). Only SERIAL and BOOTSEL are non-OTA upload paths.
    if port_type in (PortType.SERIAL, PortType.BOOTSEL) and getattr(
        args, "partition_table", False
    ):
        raise EsphomeError(
            "The option --partition-table can only be used for Over The Air updates."
        )

    if port_type == PortType.BOOTSEL:
        exit_code = upload_using_picotool(config)
        # Return None for device - BOOTSEL can't be used for logging,
        # so command_run will show the interactive chooser for log source
        return exit_code, None

    if port_type == PortType.SERIAL:
        check_permissions(host)

        exit_code = 1
        if CORE.target_platform in (PLATFORM_ESP32, PLATFORM_ESP8266):
            file = getattr(args, "file", None)
            exit_code = upload_using_esptool(config, host, file, args.upload_speed)
        elif CORE.target_platform == PLATFORM_RP2040 or CORE.is_libretiny:
            exit_code = upload_using_platformio(config, host)
        # else: Unknown target platform, exit_code remains 1

        return exit_code, host if exit_code == 0 else None

    requested_platform = getattr(args, "ota_platform", None)
    chosen_platform = _choose_ota_platform(config, requested_platform)

    # Resolve MQTT magic strings to actual IP addresses
    network_devices = _resolve_network_devices(devices, config, args)

    if chosen_platform == CONF_WEB_SERVER:
        if getattr(args, "partition_table", False):
            raise EsphomeError(
                "--partition-table is only supported with the esphome OTA platform; "
                "the web_server OTA path can only update the firmware image."
            )
        binary = CORE.firmware_bin
        if getattr(args, "file", None) is not None:
            binary = Path(args.file)
        return _upload_via_web_server(config, network_devices, binary)

    return _upload_via_native_api(config, network_devices, args)


def _choose_ota_platform(config: ConfigType, requested: str | None) -> str:
    """Pick the OTA platform to use, optionally honoring ``--ota-platform``.

    Default behavior prefers ``esphome`` (native API) when it is configured.
    The native API uses challenge-response auth with MD5/SHA256 hashing of a
    server-issued nonce, so the password is never sent over the wire; the
    ``web_server`` path uses HTTP Basic auth which transmits credentials in
    cleartext over the LAN. (The native path also supports gzip compression
    on ESP8266, where flash space is tight; on ESP32/RP2040/LibreTiny the
    backend reports ``supports_compression() == false`` and the firmware is
    sent uncompressed regardless of which platform is used.) Falls back to
    ``web_server`` only when that is the only available platform.
    """
    # Use a dict (insertion-ordered) instead of a list so error messages and
    # membership checks see one entry per platform even if the user has
    # multiple ``ota:`` items of the same platform; the web_server OTA
    # platform's final-validate hook merges duplicates anyway.
    available: dict[str, None] = {}
    for ota_item in config.get(CONF_OTA, []):
        platform = ota_item.get(CONF_PLATFORM)
        if platform in (CONF_ESPHOME, CONF_WEB_SERVER):
            available[platform] = None

    if not available:
        raise EsphomeError(
            f"Cannot upload Over the Air as the {CONF_OTA} configuration is not "
            f"present or does not include {CONF_PLATFORM}: {CONF_ESPHOME} or "
            f"{CONF_PLATFORM}: {CONF_WEB_SERVER}"
        )

    if requested is not None:
        if requested not in available:
            raise EsphomeError(
                f"--ota-platform {requested} was requested but the configuration "
                f"only provides: {', '.join(available)}"
            )
        return requested

    if CONF_ESPHOME in available:
        return CONF_ESPHOME
    return CONF_WEB_SERVER


def _upload_via_native_api(
    config: ConfigType, network_devices: list[str], args: ArgsProtocol
) -> tuple[int, str | None]:
    ota_conf: ConfigType = {}
    for ota_item in config.get(CONF_OTA, []):
        if ota_item.get(CONF_PLATFORM) == CONF_ESPHOME:
            ota_conf = ota_item
            break

    from esphome import espota2

    remote_port = int(ota_conf[CONF_PORT])
    password = ota_conf.get(CONF_PASSWORD)

    binary = CORE.firmware_bin
    ota_type = espota2.OTA_TYPE_UPDATE_APP
    if getattr(args, "partition_table", False):
        # Fail fast if the resolved ESPHome OTA config does not enable allow_partition_access.
        # The device-side handshake also rejects this with "Device only supports app updates",
        # but checking here surfaces the misconfiguration before opening a network connection.
        if not ota_conf.get("allow_partition_access"):
            raise EsphomeError(
                "The option --partition-table requires 'allow_partition_access: true' on the "
                "esphome OTA platform in the device's YAML configuration. Add it, recompile, "
                "flash a build with the option enabled, and then retry --partition-table."
            )
        binary = CORE.partition_table_bin
        ota_type = espota2.OTA_TYPE_UPDATE_PARTITION_TABLE
    if getattr(args, "file", None) is not None:
        binary = Path(args.file)

    if ota_type == espota2.OTA_TYPE_UPDATE_PARTITION_TABLE:
        _validate_partition_table_binary(binary)

    return espota2.run_ota(network_devices, remote_port, password, binary, ota_type)


def _upload_via_web_server(
    config: ConfigType, network_devices: list[str], binary: Path
) -> tuple[int, str | None]:
    web_conf = config.get(CONF_WEB_SERVER)
    if not web_conf:
        raise EsphomeError(
            f"Cannot upload via web_server OTA: the {CONF_WEB_SERVER} component "
            f"is not configured."
        )

    remote_port = int(web_conf[CONF_PORT])
    auth = web_conf.get(CONF_AUTH) or {}
    username = auth.get(CONF_USERNAME)
    password = auth.get(CONF_PASSWORD)

    from esphome import web_server_ota

    return web_server_ota.run_ota(
        network_devices, remote_port, username, password, binary
    )


# Layout of esp_partition_info_t on flash. Each entry is 32 bytes, leading with a
# 16-bit little-endian magic. ESP-IDF defines ESP_PARTITION_MAGIC = 0x50AA (stored as
# bytes 0xAA, 0x50) for partition entries and ESP_PARTITION_MAGIC_MD5 = 0xEBEB for the
# trailing checksum entry. Padding past the last entry is 0xFF. The full table is
# exactly ESP_PARTITION_TABLE_MAX_LEN bytes.
_PARTITION_TABLE_MAX_LEN = 0xC00
_ESP_PARTITION_MAGIC = 0x50AA
_ESP_PARTITION_MAGIC_MD5 = 0xEBEB


def _validate_partition_table_binary(binary: Path) -> None:
    """Validate that ``binary`` looks like an ESP32 partition table image.

    Catches common mistakes (wrong file, truncated build output, swapped --file path)
    before opening a network connection so the failure mode is a clear local error
    instead of a post-handshake device rejection.
    """
    try:
        data = binary.read_bytes()
    except OSError as err:
        raise EsphomeError(
            f"Cannot read partition table file '{binary}': {err}"
        ) from err

    if len(data) != _PARTITION_TABLE_MAX_LEN:
        raise EsphomeError(
            f"Partition table file '{binary}' has wrong size: expected "
            f"{_PARTITION_TABLE_MAX_LEN} bytes, got {len(data)}. "
            "Pass the partition table image (e.g. partitions.bin / partition-table.bin), "
            "not the firmware image."
        )

    first_magic = data[0] | (data[1] << 8)
    if first_magic != _ESP_PARTITION_MAGIC:
        raise EsphomeError(
            f"Partition table file '{binary}' does not start with the expected "
            f"partition magic 0x{_ESP_PARTITION_MAGIC:04X} (got 0x{first_magic:04X}). "
            "This file does not look like an ESP32 partition table."
        )

    # The MD5 checksum entry is required: without it the device-side
    # esp_partition_table_verify will accept the table but the bootloader will
    # refuse to boot from it. Scan the 32-byte entries for the MD5 magic.
    if not any(
        (data[off] | (data[off + 1] << 8)) == _ESP_PARTITION_MAGIC_MD5
        for off in range(0, _PARTITION_TABLE_MAX_LEN, 32)
    ):
        raise EsphomeError(
            f"Partition table file '{binary}' is missing the MD5 checksum entry. "
            "Regenerate the partition table with gen_esp32part.py or rebuild the project."
        )


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
        _wait_for_serial_port(port)
        check_permissions(port)
        return run_miniterm(config, port, args)

    # Check if we should use API for logging
    # Resolve MQTT magic strings to actual IP addresses
    if has_api() and (
        network_devices := _resolve_network_devices(devices, config, args)
    ):
        from esphome.components.api.client import run_logs

        return run_logs(
            config,
            network_devices,
            subscribe_states=not getattr(args, "no_states", False),
        )

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
    from esphome import yaml_util

    if not CORE.verbose:
        config = strip_default_ids(config)
    output = yaml_util.dump(config, args.show_secrets)
    # add the console decoration so the front-end can hide the secrets
    if not args.show_secrets:
        output = re.sub(
            r"(password|key|psk|ssid)\: (.+)", r"\1: \\033[8m\2\\033[28m", output
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
    native_idf = getattr(args, "native_idf", False)
    exit_code = write_cpp(config, native_idf=native_idf)
    if exit_code != 0:
        return exit_code
    if args.only_generate:
        _LOGGER.info("Successfully generated source code.")
        return 0
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    if CORE.is_host:
        from esphome.platformio_api import get_idedata

        program_path = str(get_idedata(config).firmware_elf_path)
        _LOGGER.info("Successfully compiled program to path '%s'", program_path)
    else:
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
    native_idf = getattr(args, "native_idf", False)
    exit_code = write_cpp(config, native_idf=native_idf)
    if exit_code != 0:
        return exit_code
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info("Successfully compiled program.")
    if CORE.is_host:
        from esphome.platformio_api import get_idedata

        program_path = str(get_idedata(config).firmware_elf_path)
        _LOGGER.info("Running program from path '%s'", program_path)
        return run_external_process(program_path)

    # Get devices, resolving special identifiers like OTA
    devices = choose_upload_log_host(
        default=args.device,
        check_default=None,
        purpose=Purpose.UPLOADING,
    )

    # Snapshot current serial ports before upload so we can detect new ones
    pre_upload_ports = {p.path for p in get_serial_ports()}

    exit_code, successful_device = upload_program(config, args, devices)
    if exit_code == 0:
        _LOGGER.info("Successfully uploaded program.")
    else:
        _LOGGER.warning("Failed to upload to %s", devices)
        return exit_code

    if args.no_logs:
        return 0

    # After BOOTSEL upload, wait for a new serial port to appear
    # so it shows up in the log chooser
    if (
        successful_device is None
        and CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM) == PLATFORM_RP2040
    ):
        _wait_for_serial_port(known_ports=pre_upload_ports)
        # If exactly one new serial port appeared, use it directly
        serial_ports = get_serial_ports()
        new_ports = [p for p in serial_ports if p.path not in pre_upload_ports]
        if len(new_ports) == 1:
            successful_device = new_ports[0].path

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
    from esphome import writer

    try:
        writer.clean_all(args.configuration)
    except OSError as err:
        _LOGGER.error("Error cleaning all files: %s", err)
        return 1
    _LOGGER.info("Done!")
    return 0


def command_version(args: ArgsProtocol) -> int | None:
    safe_print(f"Version: {const.__version__}")
    return 0


def command_clean(args: ArgsProtocol, config: ConfigType) -> int | None:
    from esphome import writer

    try:
        writer.clean_build()
    except OSError as err:
        _LOGGER.error("Error deleting build files: %s", err)
        return 1
    _LOGGER.info("Done!")
    return 0


def command_bundle(args: ArgsProtocol, config: ConfigType) -> int | None:
    from esphome.bundle import BUNDLE_EXTENSION, ConfigBundleCreator

    creator = ConfigBundleCreator(config)

    if args.list_only:
        files = creator.discover_files()
        for bf in sorted(files, key=lambda f: f.path):
            safe_print(f"  {bf.path}")
        _LOGGER.info("Found %d files", len(files))
        return 0

    result = creator.create_bundle()

    if args.output:
        output_path = Path(args.output)
    else:
        stem = CORE.config_path.stem
        output_path = CORE.config_dir / f"{stem}{BUNDLE_EXTENSION}"

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(result.data)

    _LOGGER.info(
        "Bundle created: %s (%d files, %.1f KB)",
        output_path,
        len(result.files),
        len(result.data) / 1024,
    )
    return 0


def command_dashboard(args: ArgsProtocol) -> int | None:
    from esphome.dashboard import dashboard

    return dashboard.start_dashboard(args)


def run_multiple_configs(
    files: list, command_builder: Callable[[str], list[str]]
) -> int:
    """Run a command for each configuration file in a subprocess.

    Args:
        files: List of configuration files to process.
        command_builder: Callable that takes a file path and returns a command list.

    Returns:
        Number of failed files.
    """
    import click

    success = {}
    twidth = 60

    def print_bar(middle_text):
        middle_text = f" {middle_text} "
        width = len(click.unstyle(middle_text))
        half_line = "=" * ((twidth - width) // 2)
        safe_print(f"{half_line}{middle_text}{half_line}")

    for f in files:
        f_path = Path(f) if not isinstance(f, Path) else f

        if any(f_path.name == x for x in SECRETS_FILES):
            _LOGGER.warning("Skipping secrets file %s", f_path)
            continue

        safe_print(f"Processing {color(AnsiFore.CYAN, str(f))}")
        safe_print("-" * twidth)
        safe_print()

        cmd = command_builder(f)
        rc = run_external_process(*cmd)

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
        if f not in success:
            continue  # Skipped file
        if success[f]:
            safe_print(f"  - {str(f)}: {color(AnsiFore.GREEN, 'SUCCESS')}")
        else:
            safe_print(f"  - {str(f)}: {color(AnsiFore.BOLD_RED, 'FAILED')}")
            failed += 1
    return failed


def command_update_all(args: ArgsProtocol) -> int | None:
    files = list_yaml_files(args.configuration)

    def build_command(f):
        dashboard = ["--dashboard"] if CORE.dashboard else []
        return [*ESPHOME_COMMAND, *dashboard, "run", f, "--no-logs", "--device", "OTA"]

    return run_multiple_configs(files, build_command)


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
        idedata=idedata,
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
    from esphome import yaml_util

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

    rc = run_external_process(*ESPHOME_COMMAND, "config", str(new_path))
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
        rc = run_external_process(*ESPHOME_COMMAND, *cli_args)
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
    "idedata": command_idedata,
    "rename": command_rename,
    "discover": command_discover,
    "analyze-memory": command_analyze_memory,
    "bundle": command_bundle,
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
    parser_compile.add_argument(
        "--native-idf",
        help="Build with native ESP-IDF instead of PlatformIO (ESP32 esp-idf framework only).",
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
        help=ARGUMENT_HELP_DEVICE,
    )
    parser_upload.add_argument(
        "--upload_speed",
        help="Override the default or configured upload speed.",
    )
    parser_upload.add_argument(
        "--file",
        help="Manually specify the binary file to upload.",
    )
    parser_upload.add_argument(
        "--ota-platform",
        choices=[CONF_ESPHOME, CONF_WEB_SERVER],
        help=(
            "OTA platform to use for network uploads. Defaults to "
            f"'{CONF_ESPHOME}' (native API) when configured because it uses "
            "challenge-response auth so the password is never sent in "
            f"cleartext on the wire. Falls back to '{CONF_WEB_SERVER}' "
            "(HTTP Basic auth) when that is the only configured platform."
        ),
    )
    parser_upload.add_argument(
        "--partition-table",
        help="Upload as partition table (OTA).",
        action="store_true",
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
        help=ARGUMENT_HELP_DEVICE,
    )
    parser_logs.add_argument(
        "--reset",
        "-r",
        action="store_true",
        help="Reset the device before starting serial logs.",
        default=os.getenv("ESPHOME_SERIAL_LOGGING_RESET"),
    )
    parser_logs.add_argument(
        "--no-states",
        action="store_true",
        help="Do not show entity state changes in log output.",
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
        help=ARGUMENT_HELP_DEVICE,
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
    parser_run.add_argument(
        "--native-idf",
        help="Build with native ESP-IDF instead of PlatformIO (ESP32 esp-idf framework only).",
        action="store_true",
    )
    parser_run.add_argument(
        "--ota-platform",
        choices=[CONF_ESPHOME, CONF_WEB_SERVER],
        help=(
            "OTA platform to use for network uploads. Defaults to "
            f"'{CONF_ESPHOME}' (native API) when configured because it uses "
            "challenge-response auth so the password is never sent in "
            f"cleartext on the wire. Falls back to '{CONF_WEB_SERVER}' "
            "(HTTP Basic auth) when that is the only configured platform."
        ),
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

    parser_bundle = subparsers.add_parser(
        "bundle",
        help="Create a self-contained config bundle for remote compilation.",
    )
    parser_bundle.add_argument(
        "configuration", help="Your YAML configuration file(s).", nargs="+"
    )
    parser_bundle.add_argument(
        "-o",
        "--output",
        help="Output path for the bundle archive.",
    )
    parser_bundle.add_argument(
        "--list-only",
        help="List discovered files without creating the archive.",
        action="store_true",
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

    # Multiple configurations: use subprocesses to avoid state leakage
    # between compilations (e.g., LVGL touchscreen state in module globals)
    if len(args.configuration) > 1:
        # Build command by reusing argv, replacing all configs with single file
        # argv[0] is the program path, skip it since we prefix with "esphome"
        def build_command(f):
            return (
                [*ESPHOME_COMMAND]
                + [arg for arg in argv[1:] if arg not in args.configuration]
                + [str(f)]
            )

        return run_multiple_configs(args.configuration, build_command)

    # Single configuration
    conf_path = Path(args.configuration[0])
    if any(conf_path.name == x for x in SECRETS_FILES):
        _LOGGER.warning("Skipping secrets file %s", conf_path)
        return 0

    # Bundle support: if the configuration is a .esphomebundle, extract it
    # and rewrite conf_path to the extracted YAML config.
    from esphome.bundle import is_bundle_path, prepare_bundle_for_compile

    if is_bundle_path(conf_path):
        _LOGGER.info("Extracting config bundle %s...", conf_path)
        conf_path = prepare_bundle_for_compile(conf_path)
        # Update the argument so downstream code sees the extracted path
        args.configuration[0] = str(conf_path)

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
        return 1

    try:
        return POST_CONFIG_ACTIONS[args.command](args, config)
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
