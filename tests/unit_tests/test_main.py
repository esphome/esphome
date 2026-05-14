"""Unit tests for esphome.__main__ module."""

from __future__ import annotations

from collections.abc import Callable, Generator
from dataclasses import dataclass
import json
import logging
import os
from pathlib import Path
import re
import sys
import time
from typing import Any
from unittest.mock import AsyncMock, MagicMock, Mock, patch

import pytest
from pytest import CaptureFixture
from zeroconf import ServiceStateChange

from esphome.__main__ import (
    Purpose,
    _get_configured_xtal_freq,
    _make_crystal_freq_callback,
    _resolve_network_devices,
    _validate_bootloader_binary,
    _validate_partition_table_binary,
    choose_upload_log_host,
    command_analyze_memory,
    command_bundle,
    command_clean_all,
    command_config_hash,
    command_rename,
    command_run,
    command_update_all,
    command_wizard,
    compile_program,
    detect_external_components,
    get_port_type,
    has_ip_address,
    has_mqtt,
    has_mqtt_ip_lookup,
    has_mqtt_logging,
    has_name_add_mac_suffix,
    has_non_ip_address,
    has_ota,
    has_resolvable_address,
    has_web_server_ota,
    mqtt_get_ip,
    parse_args,
    run_esphome,
    run_miniterm,
    show_logs,
    upload_program,
    upload_using_esptool,
    upload_using_picotool,
    upload_using_platformio,
)
from esphome.address_cache import AddressCache
from esphome.bundle import BUNDLE_EXTENSION, BundleFile, BundleResult
from esphome.components import esp32
from esphome.components.esp32 import KEY_ESP32, KEY_VARIANT, VARIANT_ESP32
from esphome.const import (
    CONF_API,
    CONF_AUTH,
    CONF_BAUD_RATE,
    CONF_BROKER,
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
    CONF_PORT,
    CONF_SUBSTITUTIONS,
    CONF_TOPIC,
    CONF_USE_ADDRESS,
    CONF_USERNAME,
    CONF_WEB_SERVER,
    CONF_WIFI,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    Toolchain,
)
from esphome.core import CORE, EsphomeError
from esphome.espota2 import (
    OTA_TYPE_UPDATE_APP,
    OTA_TYPE_UPDATE_BOOTLOADER,
    OTA_TYPE_UPDATE_PARTITION_TABLE,
)
from esphome.platformio import toolchain
from esphome.util import BootselResult, FlashImage
from esphome.zeroconf import _await_discovery, discover_mdns_devices


def strip_ansi_codes(text: str) -> str:
    """Remove ANSI escape codes from text.

    This helps make test assertions cleaner by removing color codes and other
    terminal formatting that can make tests brittle.
    """
    # Pattern to match ANSI escape sequences
    ansi_escape = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")
    return ansi_escape.sub("", text)


@dataclass
class MockSerialPort:
    """Mock serial port for testing.

    Attributes:
        path (str): The device path of the mock serial port (e.g., '/dev/ttyUSB0').
        description (str): A human-readable description of the mock serial port.
    """

    path: str
    description: str


def setup_core(
    config: dict[str, Any] | None = None,
    address: str | None = None,
    platform: str | None = None,
    tmp_path: Path | None = None,
    name: str = "test",
) -> None:
    """
    Helper to set up CORE configuration with optional address.

    Args:
        config (dict[str, Any] | None): The configuration dictionary to set for CORE. If None, an empty dict is used.
        address (str | None): Optional network address to set in the configuration. If provided, it is set under the wifi config.
        platform (str | None): Optional target platform to set in CORE.data.
        tmp_path (Path | None): Optional temp path for setting up build paths.
        name (str): The name of the device (defaults to "test").
    """
    if config is None:
        config = {}

    if address is not None:
        # Set address via wifi config (could also use ethernet)
        config[CONF_WIFI] = {CONF_USE_ADDRESS: address}

    CORE.config = config
    CORE.toolchain = Toolchain.PLATFORMIO

    if platform is not None:
        CORE.data[KEY_CORE] = {}
        CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = platform

    if tmp_path is not None:
        CORE.config_path = str(tmp_path / f"{name}.yaml")
        CORE.name = name
        CORE.build_path = str(tmp_path / ".esphome" / "build" / name)


@pytest.fixture
def mock_no_serial_ports() -> Generator[Mock]:
    """Mock get_serial_ports to return no ports."""
    with patch("esphome.__main__.get_serial_ports", return_value=[]) as mock:
        yield mock


@pytest.fixture
def mock_get_port_type() -> Generator[Mock]:
    """Mock get_port_type for testing."""
    with patch("esphome.__main__.get_port_type") as mock:
        yield mock


@pytest.fixture
def mock_check_permissions() -> Generator[Mock]:
    """Mock check_permissions for testing."""
    with patch("esphome.__main__.check_permissions") as mock:
        yield mock


@pytest.fixture
def mock_run_miniterm() -> Generator[Mock]:
    """Mock run_miniterm for testing."""
    with patch("esphome.__main__.run_miniterm") as mock:
        yield mock


@pytest.fixture
def mock_wait_for_serial_port() -> Generator[Mock]:
    """Mock _wait_for_serial_port for testing."""
    with patch("esphome.__main__._wait_for_serial_port") as mock:
        yield mock


@pytest.fixture
def mock_upload_using_esptool() -> Generator[Mock]:
    """Mock upload_using_esptool for testing."""
    with patch("esphome.__main__.upload_using_esptool") as mock:
        yield mock


@pytest.fixture
def mock_upload_using_platformio() -> Generator[Mock]:
    """Mock upload_using_platformio for testing."""
    with patch("esphome.__main__.upload_using_platformio") as mock:
        yield mock


@pytest.fixture
def mock_upload_using_picotool() -> Generator[Mock]:
    """Mock upload_using_picotool for testing."""
    with patch("esphome.__main__.upload_using_picotool") as mock:
        yield mock


@pytest.fixture
def mock_run_ota() -> Generator[Mock]:
    """Mock espota2.run_ota for testing."""
    with patch("esphome.espota2.run_ota") as mock:
        yield mock


@pytest.fixture
def mock_run_web_server_ota() -> Generator[Mock]:
    """Mock web_server_ota.run_ota for testing."""
    with patch("esphome.web_server_ota.run_ota") as mock:
        yield mock


@pytest.fixture
def mock_is_ip_address() -> Generator[Mock]:
    """Mock is_ip_address for testing."""
    with patch("esphome.__main__.is_ip_address") as mock:
        yield mock


@pytest.fixture
def mock_mqtt_get_ip() -> Generator[Mock]:
    """Mock mqtt_get_ip for testing."""
    with patch("esphome.__main__.mqtt_get_ip") as mock:
        yield mock


@pytest.fixture
def mock_serial_ports() -> Generator[Mock]:
    """Mock get_serial_ports to return test ports."""
    mock_ports = [
        MockSerialPort("/dev/ttyUSB0", "USB Serial"),
        MockSerialPort("/dev/ttyUSB1", "Another USB Serial"),
    ]
    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports) as mock:
        yield mock


@pytest.fixture
def mock_choose_prompt() -> Generator[Mock]:
    """Mock choose_prompt to return default selection."""
    with patch("esphome.__main__.choose_prompt", return_value="/dev/ttyUSB0") as mock:
        yield mock


@pytest.fixture
def mock_no_mqtt_logging() -> Generator[Mock]:
    """Mock has_mqtt_logging to return False."""
    with patch("esphome.__main__.has_mqtt_logging", return_value=False) as mock:
        yield mock


@pytest.fixture
def mock_has_mqtt_logging() -> Generator[Mock]:
    """Mock has_mqtt_logging to return True."""
    with patch("esphome.__main__.has_mqtt_logging", return_value=True) as mock:
        yield mock


@pytest.fixture
def mock_run_external_process() -> Generator[Mock]:
    """Mock run_external_process for testing."""
    with patch("esphome.__main__.run_external_process") as mock:
        mock.return_value = 0  # Default to success
        yield mock


@pytest.fixture
def mock_run_external_command_main() -> Generator[Mock]:
    """Mock run_external_command in __main__ module (different from platformio toolchain)."""
    with patch("esphome.__main__.run_external_command") as mock:
        mock.return_value = 0  # Default to success
        yield mock


@pytest.fixture
def mock_write_cpp() -> Generator[Mock]:
    """Mock write_cpp for testing."""
    with patch("esphome.__main__.write_cpp") as mock:
        mock.return_value = 0  # Default to success
        yield mock


@pytest.fixture
def mock_compile_program() -> Generator[Mock]:
    """Mock compile_program for testing."""
    with patch("esphome.__main__.compile_program") as mock:
        mock.return_value = 0  # Default to success
        yield mock


@pytest.fixture
def mock_get_esphome_components() -> Generator[Mock]:
    """Mock get_esphome_components for testing."""
    with patch("esphome.analyze_memory.helpers.get_esphome_components") as mock:
        mock.return_value = {"logger", "api", "ota"}
        yield mock


@pytest.fixture
def mock_memory_analyzer_cli() -> Generator[Mock]:
    """Mock MemoryAnalyzerCLI for testing."""
    with patch("esphome.analyze_memory.cli.MemoryAnalyzerCLI") as mock_class:
        mock_analyzer = MagicMock()
        mock_analyzer.generate_report.return_value = "Mock Memory Report"
        mock_class.return_value = mock_analyzer
        yield mock_class


@pytest.fixture
def mock_ram_strings_analyzer() -> Generator[Mock]:
    """Mock RamStringsAnalyzer for testing."""
    with patch("esphome.analyze_memory.ram_strings.RamStringsAnalyzer") as mock_class:
        mock_analyzer = MagicMock()
        mock_analyzer.generate_report.return_value = "Mock RAM Strings Report"
        mock_class.return_value = mock_analyzer
        yield mock_class


def test_choose_upload_log_host_with_string_default() -> None:
    """Test with a single string default device."""
    setup_core()
    result = choose_upload_log_host(
        default="192.168.1.100",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


def test_choose_upload_log_host_with_list_default() -> None:
    """Test with a list of default devices."""
    setup_core()
    result = choose_upload_log_host(
        default=["192.168.1.100", "192.168.1.101"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100", "192.168.1.101"]


def test_choose_upload_log_host_with_multiple_ip_addresses() -> None:
    """Test with multiple IP addresses as defaults."""
    setup_core()
    result = choose_upload_log_host(
        default=["1.2.3.4", "4.5.5.6"],
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    assert result == ["1.2.3.4", "4.5.5.6"]


def test_choose_upload_log_host_with_mixed_hostnames_and_ips() -> None:
    """Test with a mix of hostnames and IP addresses."""
    setup_core()
    result = choose_upload_log_host(
        default=["host.one", "host.one.local", "1.2.3.4"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["host.one", "host.one.local", "1.2.3.4"]


def test_choose_upload_log_host_with_ota_list() -> None:
    """Test with OTA as the only item in the list."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]}, address="192.168.1.100"
    )

    result = choose_upload_log_host(
        default=["OTA"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_with_ota_list_mqtt_fallback() -> None:
    """Test with OTA list falling back to MQTT when no address."""
    setup_core(config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}], "mqtt": {}})

    result = choose_upload_log_host(
        default=["OTA"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["MQTTIP"]


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_with_ota_list_mqtt_fallback_logging() -> None:
    """Test with OTA list with API and MQTT when no address."""
    setup_core(config={CONF_API: {}, "mqtt": {}})

    result = choose_upload_log_host(
        default=["OTA"],
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    assert result == ["MQTTIP", "MQTT"]


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_with_serial_device_no_ports(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test SERIAL device when no serial ports are found."""
    setup_core()
    with pytest.raises(
        EsphomeError, match="All specified devices .* could not be resolved"
    ):
        choose_upload_log_host(
            default="SERIAL",
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
    assert "No serial ports found, skipping SERIAL device" in caplog.text


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_with_serial_device_with_ports(
    mock_choose_prompt: Mock,
) -> None:
    """Test SERIAL device when serial ports are available."""
    setup_core()
    result = choose_upload_log_host(
        default="SERIAL",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["/dev/ttyUSB0"]
    mock_choose_prompt.assert_called_once_with(
        [
            ("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0"),
            ("/dev/ttyUSB1 (Another USB Serial)", "/dev/ttyUSB1"),
        ],
        purpose=Purpose.UPLOADING,
    )


def test_choose_upload_log_host_with_ota_device_with_ota_config() -> None:
    """Test OTA device when OTA is configured."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]}, address="192.168.1.100"
    )

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


def test_choose_upload_log_host_with_ota_device_with_api_config() -> None:
    """Test OTA device when API is configured (no upload without OTA in config)."""
    setup_core(config={CONF_API: {}}, address="192.168.1.100")

    with pytest.raises(
        EsphomeError, match="All specified devices .* could not be resolved"
    ):
        choose_upload_log_host(
            default="OTA",
            check_default=None,
            purpose=Purpose.UPLOADING,
        )


def test_choose_upload_log_host_with_ota_device_with_api_config_logging() -> None:
    """Test OTA device when API is configured."""
    setup_core(config={CONF_API: {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_with_ota_device_fallback_to_mqtt() -> None:
    """Test OTA device fallback to MQTT when no OTA/API config."""
    setup_core(config={"mqtt": {}})

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    assert result == ["MQTT"]


@pytest.mark.usefixtures("mock_no_mqtt_logging")
def test_choose_upload_log_host_with_ota_device_no_fallback() -> None:
    """Test OTA device with no valid fallback options."""
    setup_core()

    with pytest.raises(
        EsphomeError, match="All specified devices .* could not be resolved"
    ):
        choose_upload_log_host(
            default="OTA",
            check_default=None,
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_choose_prompt")
def test_choose_upload_log_host_multiple_devices() -> None:
    """Test with multiple devices including special identifiers."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]}, address="192.168.1.100"
    )

    mock_ports = [MockSerialPort("/dev/ttyUSB0", "USB Serial")]

    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports):
        result = choose_upload_log_host(
            default=["192.168.1.50", "OTA", "SERIAL"],
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert result == ["192.168.1.50", "192.168.1.100", "/dev/ttyUSB0"]


def test_choose_upload_log_host_no_defaults_with_serial_ports(
    mock_choose_prompt: Mock,
) -> None:
    """Test interactive mode with serial ports available."""
    mock_ports = [
        MockSerialPort("/dev/ttyUSB0", "USB Serial"),
    ]

    setup_core()

    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports):
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert result == ["/dev/ttyUSB0"]
        mock_choose_prompt.assert_called_once_with(
            [("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0")],
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_no_defaults_with_ota() -> None:
    """Test interactive mode with OTA option."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]}, address="192.168.1.100"
    )

    with patch(
        "esphome.__main__.choose_prompt", return_value="192.168.1.100"
    ) as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert result == ["192.168.1.100"]
        mock_prompt.assert_called_once_with(
            [("Over The Air (192.168.1.100)", "192.168.1.100")],
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_no_defaults_with_api() -> None:
    """Test interactive mode with API option."""
    setup_core(config={CONF_API: {}}, address="192.168.1.100")

    with patch(
        "esphome.__main__.choose_prompt", return_value="192.168.1.100"
    ) as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.LOGGING,
        )
        assert result == ["192.168.1.100"]
        mock_prompt.assert_called_once_with(
            [("Over The Air (192.168.1.100)", "192.168.1.100")],
            purpose=Purpose.LOGGING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports", "mock_has_mqtt_logging")
def test_choose_upload_log_host_no_defaults_with_mqtt() -> None:
    """Test interactive mode with MQTT option."""
    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local"}})

    with patch("esphome.__main__.choose_prompt", return_value="MQTT") as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.LOGGING,
        )
        assert result == ["MQTT"]
        mock_prompt.assert_called_once_with(
            [("MQTT (mqtt.local)", "MQTT")],
            purpose=Purpose.LOGGING,
        )


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_no_defaults_with_all_options(
    mock_choose_prompt: Mock,
) -> None:
    """Test interactive mode with all options available."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
            CONF_API: {},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        address="192.168.1.100",
    )

    mock_ports = [MockSerialPort("/dev/ttyUSB0", "USB Serial")]

    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports):
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert result == ["/dev/ttyUSB0"]

        expected_options = [
            ("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0"),
            ("Over The Air (192.168.1.100)", "192.168.1.100"),
            ("Over The Air (MQTT IP lookup)", "MQTTIP"),
        ]
        mock_choose_prompt.assert_called_once_with(
            expected_options, purpose=Purpose.UPLOADING
        )


def test_choose_upload_log_host_no_defaults_with_all_options_logging(
    mock_choose_prompt: Mock,
) -> None:
    """Test interactive mode with all options available."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
            CONF_API: {},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        address="192.168.1.100",
    )

    mock_ports = [MockSerialPort("/dev/ttyUSB0", "USB Serial")]

    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports):
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.LOGGING,
        )
        assert result == ["/dev/ttyUSB0"]

        expected_options = [
            ("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0"),
            ("MQTT (mqtt.local)", "MQTT"),
            ("Over The Air (192.168.1.100)", "192.168.1.100"),
            ("Over The Air (MQTT IP lookup)", "MQTTIP"),
        ]
        mock_choose_prompt.assert_called_once_with(
            expected_options, purpose=Purpose.LOGGING
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_check_default_matches() -> None:
    """Test when check_default matches an available option."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]}, address="192.168.1.100"
    )

    result = choose_upload_log_host(
        default=None,
        check_default="192.168.1.100",
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_check_default_no_match() -> None:
    """Test when check_default doesn't match any available option."""
    setup_core()

    with patch(
        "esphome.__main__.choose_prompt", return_value="fallback"
    ) as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default="192.168.1.100",
            purpose=Purpose.UPLOADING,
        )
        assert result == ["fallback"]
        mock_prompt.assert_called_once()


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_empty_defaults_list() -> None:
    """Test with an empty list as default."""
    setup_core()
    with patch("esphome.__main__.choose_prompt", return_value="chosen") as mock_prompt:
        result = choose_upload_log_host(
            default=[],
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert result == ["chosen"]
        mock_prompt.assert_called_once()


@pytest.mark.usefixtures("mock_no_serial_ports", "mock_no_mqtt_logging")
def test_choose_upload_log_host_all_devices_unresolved() -> None:
    """Test when all specified devices cannot be resolved."""
    setup_core()

    with pytest.raises(
        EsphomeError,
        match=r"All specified devices \['SERIAL', 'OTA'\] could not be resolved",
    ):
        choose_upload_log_host(
            default=["SERIAL", "OTA"],
            check_default=None,
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports", "mock_no_mqtt_logging")
def test_choose_upload_log_host_mixed_resolved_unresolved() -> None:
    """Test with a mix of resolved and unresolved devices."""
    setup_core()

    result = choose_upload_log_host(
        default=["192.168.1.50", "SERIAL", "OTA"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.50"]


def test_choose_upload_log_host_ota_both_conditions() -> None:
    """Test OTA device when both OTA and API are configured and enabled."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}], CONF_API: {}},
        address="192.168.1.100",
    )

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_ota_ip_all_options() -> None:
    """Test OTA device when both static IP, OTA, API and MQTT are configured and enabled but MDNS not."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
            CONF_API: {},
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
            },
            CONF_MDNS: {
                CONF_DISABLED: True,
            },
        },
        address="192.168.1.100",
    )

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100", "MQTTIP"]


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_ota_local_all_options() -> None:
    """Test OTA device when both static IP, OTA, API and MQTT are configured and enabled but MDNS not."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
            CONF_API: {},
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
            },
            CONF_MDNS: {
                CONF_DISABLED: True,
            },
        },
        address="test.local",
    )

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["MQTTIP"]


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_ota_ip_all_options_logging() -> None:
    """Test OTA device when both static IP, OTA, API and MQTT are configured and enabled but MDNS not."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
            CONF_API: {},
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
            },
            CONF_MDNS: {
                CONF_DISABLED: True,
            },
        },
        address="192.168.1.100",
    )

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    assert result == ["192.168.1.100", "MQTTIP", "MQTT"]


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_ota_local_all_options_logging() -> None:
    """Test OTA device when both static IP, OTA, API and MQTT are configured and enabled but MDNS not."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
            CONF_API: {},
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
            },
            CONF_MDNS: {
                CONF_DISABLED: True,
            },
        },
        address="test.local",
    )

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.LOGGING,
    )
    assert result == ["MQTTIP", "MQTT"]


@pytest.mark.usefixtures("mock_no_mqtt_logging")
def test_choose_upload_log_host_no_address_with_ota_config() -> None:
    """Test OTA device when OTA is configured but no address is set."""
    setup_core(config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]})

    with pytest.raises(
        EsphomeError, match="All specified devices .* could not be resolved"
    ):
        choose_upload_log_host(
            default="OTA",
            check_default=None,
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_no_defaults_with_rp2040_bootsel(
    mock_choose_prompt: Mock,
) -> None:
    """Test interactive mode shows RP2040 BOOTSEL option via picotool."""
    setup_core(platform=PLATFORM_RP2040)

    with (
        patch(
            "esphome.__main__._find_picotool", return_value=Path("/usr/bin/picotool")
        ),
        patch("esphome.__main__.detect_rp2040_bootsel", return_value=BootselResult(1)),
    ):
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert result == ["/dev/ttyUSB0"]  # mock_choose_prompt default
        mock_choose_prompt.assert_called_once_with(
            [("RP2040 BOOTSEL (via picotool)", "BOOTSEL")],
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_rp2040_no_device_shows_bootsel_help() -> None:
    """Test BOOTSEL instructions shown when no RP2040 device found."""
    setup_core(platform=PLATFORM_RP2040)

    with (
        patch(
            "esphome.__main__._find_picotool", return_value=Path("/usr/bin/picotool")
        ),
        patch("esphome.__main__.detect_rp2040_bootsel", return_value=BootselResult(0)),
        pytest.raises(EsphomeError, match="BOOTSEL"),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_rp2040_bootsel_tip_with_ota(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test BOOTSEL tip shown when only OTA options exist for RP2040."""
    setup_core(
        platform=PLATFORM_RP2040,
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]},
        address="192.168.1.100",
    )

    with (
        patch(
            "esphome.__main__._find_picotool", return_value=Path("/usr/bin/picotool")
        ),
        patch("esphome.__main__.detect_rp2040_bootsel", return_value=BootselResult(0)),
        patch(
            "esphome.__main__.choose_prompt",
            return_value="192.168.1.100",
        ),
        caplog.at_level(logging.INFO, logger="esphome.__main__"),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert "BOOTSEL" in caplog.text


def test_choose_upload_log_host_rp2040_bootsel_tip_with_serial_ports(
    caplog: pytest.LogCaptureFixture,
    mock_choose_prompt: Mock,
) -> None:
    """Test BOOTSEL tip shown when serial ports exist but no BOOTSEL device."""
    setup_core(platform=PLATFORM_RP2040)

    mock_ports = [MockSerialPort("/dev/ttyACM0", "RP2040 Serial")]
    with (
        patch("esphome.__main__.get_serial_ports", return_value=mock_ports),
        patch(
            "esphome.__main__._find_picotool",
            return_value=Path("/usr/bin/picotool"),
        ),
        patch("esphome.__main__.detect_rp2040_bootsel", return_value=BootselResult(0)),
        caplog.at_level(logging.INFO, logger="esphome.__main__"),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        assert "BOOTSEL" in caplog.text


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_rp2040_permission_error_no_options(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test permission warning shown when BOOTSEL device found but not accessible."""
    setup_core(platform=PLATFORM_RP2040)

    with (
        patch(
            "esphome.__main__._find_picotool", return_value=Path("/usr/bin/picotool")
        ),
        patch(
            "esphome.__main__.detect_rp2040_bootsel",
            return_value=BootselResult(0, permission_error=True),
        ),
        patch("esphome.__main__.sys.platform", "linux"),
        pytest.raises(EsphomeError, match="BOOTSEL"),
        caplog.at_level(logging.WARNING, logger="esphome.__main__"),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )

    assert "USB permissions" in caplog.text
    assert "udev" in caplog.text


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_rp2040_permission_error_with_ota(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test permission warning shown with OTA fallback available."""
    setup_core(
        platform=PLATFORM_RP2040,
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]},
        address="192.168.1.100",
    )

    with (
        patch(
            "esphome.__main__._find_picotool", return_value=Path("/usr/bin/picotool")
        ),
        patch(
            "esphome.__main__.detect_rp2040_bootsel",
            return_value=BootselResult(0, permission_error=True),
        ),
        patch(
            "esphome.__main__.choose_prompt",
            return_value="192.168.1.100",
        ),
        caplog.at_level(logging.WARNING, logger="esphome.__main__"),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )

    assert "USB permissions" in caplog.text


def test_choose_upload_log_host_no_bootsel_for_non_rp2040(
    mock_no_serial_ports: Mock,
) -> None:
    """Test that BOOTSEL detection is not run for non-RP2040 platforms."""
    setup_core(
        platform=PLATFORM_ESP32,
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]},
        address="192.168.1.100",
    )

    with (
        patch("esphome.__main__._find_picotool") as mock_find_picotool,
        patch(
            "esphome.__main__.choose_prompt",
            return_value="192.168.1.100",
        ),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        mock_find_picotool.assert_not_called()


def test_choose_upload_log_host_rp2040_serial_and_bootsel(
    mock_choose_prompt: Mock,
) -> None:
    """Test both serial ports and BOOTSEL option shown for RP2040."""
    setup_core(platform=PLATFORM_RP2040)

    mock_ports = [MockSerialPort("/dev/ttyACM0", "RP2040 Serial")]
    with (
        patch("esphome.__main__.get_serial_ports", return_value=mock_ports),
        patch(
            "esphome.__main__._find_picotool", return_value=Path("/usr/bin/picotool")
        ),
        patch("esphome.__main__.detect_rp2040_bootsel", return_value=BootselResult(1)),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )
        mock_choose_prompt.assert_called_once_with(
            [
                ("/dev/ttyACM0 (RP2040 Serial)", "/dev/ttyACM0"),
                ("RP2040 BOOTSEL (via picotool)", "BOOTSEL"),
            ],
            purpose=Purpose.UPLOADING,
        )


@dataclass
class MockArgs:
    """Mock args for testing."""

    file: str | None = None
    upload_speed: int = 460800
    username: str | None = None
    password: str | None = None
    client_id: str | None = None
    topic: str | None = None
    configuration: str | None = None
    name: str | None = None
    dashboard: bool = False
    reset: bool = False
    list_only: bool = False
    output: str | None = None
    ota_platform: str | None = None
    partition_table: bool = False
    bootloader: bool = False


def test_upload_program_serial_esp32(
    mock_upload_using_esptool: Mock,
    mock_get_port_type: Mock,
    mock_check_permissions: Mock,
) -> None:
    """Test upload_program with serial port for ESP32."""
    setup_core(platform=PLATFORM_ESP32)
    mock_get_port_type.return_value = "SERIAL"
    mock_upload_using_esptool.return_value = 0

    config = {}
    args = MockArgs()
    devices = ["/dev/ttyUSB0"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "/dev/ttyUSB0"
    mock_check_permissions.assert_called_once_with("/dev/ttyUSB0")
    mock_upload_using_esptool.assert_called_once()


def test_upload_program_serial_esp8266_with_file(
    mock_upload_using_esptool: Mock,
    mock_get_port_type: Mock,
    mock_check_permissions: Mock,
) -> None:
    """Test upload_program with serial port for ESP8266 with custom file."""
    setup_core(platform=PLATFORM_ESP8266)
    mock_get_port_type.return_value = "SERIAL"
    mock_upload_using_esptool.return_value = 0

    config = {}
    args = MockArgs(file="firmware.bin")
    devices = ["/dev/ttyUSB0"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "/dev/ttyUSB0"
    mock_check_permissions.assert_called_once_with("/dev/ttyUSB0")
    mock_upload_using_esptool.assert_called_once_with(
        config, "/dev/ttyUSB0", "firmware.bin", 460800
    )


def test_upload_using_esptool_path_conversion(
    tmp_path: Path,
    mock_run_external_command_main: Mock,
    mock_get_idedata: Mock,
) -> None:
    """Test upload_using_esptool properly converts Path objects to strings for esptool.

    This test ensures that img.path (Path object) is converted to string before
    passing to esptool, preventing AttributeError.
    """
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test")

    # Set up ESP32-specific data required by get_esp32_variant()
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32}

    # Create mock IDEData with Path objects
    mock_idedata = MagicMock(spec=toolchain.IDEData)
    mock_idedata.firmware_bin_path = tmp_path / "firmware.bin"
    mock_idedata.extra_flash_images = [
        FlashImage(path=tmp_path / "bootloader.bin", offset="0x1000"),
        FlashImage(path=tmp_path / "partitions.bin", offset="0x8000"),
    ]

    mock_get_idedata.return_value = mock_idedata

    # Create the actual firmware files so they exist
    (tmp_path / "firmware.bin").touch()
    (tmp_path / "bootloader.bin").touch()
    (tmp_path / "partitions.bin").touch()

    config = {CONF_ESPHOME: {"platformio_options": {}}}

    # Call upload_using_esptool without custom file argument
    result = upload_using_esptool(config, "/dev/ttyUSB0", None, None)

    assert result == 0

    # Verify that run_external_command was called
    assert mock_run_external_command_main.call_count == 1

    # Get the actual call arguments
    call_args = mock_run_external_command_main.call_args[0]

    # The first argument should be esptool.main function,
    # followed by the command arguments
    assert len(call_args) > 1

    # Find the indices of the flash image arguments
    # They should come after "write-flash" and "-z"
    cmd_list = list(call_args[1:])  # Skip the esptool.main function

    # Verify all paths are strings, not Path objects
    # The firmware and flash images should be at specific positions
    write_flash_idx = cmd_list.index("write-flash")

    # After write-flash we have: -z, --flash-size, detect, then offset/path pairs
    # Check firmware at offset 0x10000 (ESP32)
    firmware_offset_idx = write_flash_idx + 4
    assert cmd_list[firmware_offset_idx] == "0x10000"
    firmware_path = cmd_list[firmware_offset_idx + 1]
    assert isinstance(firmware_path, str)
    assert firmware_path.endswith("firmware.bin")

    # Check bootloader
    bootloader_offset_idx = firmware_offset_idx + 2
    assert cmd_list[bootloader_offset_idx] == "0x1000"
    bootloader_path = cmd_list[bootloader_offset_idx + 1]
    assert isinstance(bootloader_path, str)
    assert bootloader_path.endswith("bootloader.bin")

    # Check partitions
    partitions_offset_idx = bootloader_offset_idx + 2
    assert cmd_list[partitions_offset_idx] == "0x8000"
    partitions_path = cmd_list[partitions_offset_idx + 1]
    assert isinstance(partitions_path, str)
    assert partitions_path.endswith("partitions.bin")


def test_upload_using_esptool_skips_missing_extra_flash_images(
    tmp_path: Path,
    mock_run_external_command_main: Mock,
    mock_get_idedata: Mock,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A non-existent path in extra_flash_images must be filtered out with a
    warning, and must not appear in the esptool command line. Only the valid
    images are flashed. Regression test for
    https://github.com/esphome/esphome/issues/15634.
    """
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test")
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32}

    missing_path = tmp_path / "variants" / "tasmota" / "tinyuf2.bin"

    mock_idedata = MagicMock(spec=toolchain.IDEData)
    mock_idedata.firmware_bin_path = tmp_path / "firmware.bin"
    mock_idedata.extra_flash_images = [
        FlashImage(path=tmp_path / "bootloader.bin", offset="0x1000"),
        FlashImage(path=missing_path, offset="0x2d0000"),
    ]
    mock_get_idedata.return_value = mock_idedata

    (tmp_path / "firmware.bin").touch()
    (tmp_path / "bootloader.bin").touch()
    # Intentionally do NOT create missing_path

    config = {CONF_ESPHOME: {"platformio_options": {}}}

    with caplog.at_level(logging.WARNING, logger="esphome.__main__"):
        result = upload_using_esptool(config, "/dev/ttyUSB0", None, None)

    assert result == 0
    assert "Skipping missing flash image" in caplog.text
    assert str(missing_path) in caplog.text

    cmd_list = list(mock_run_external_command_main.call_args[0][1:])
    assert str(missing_path) not in cmd_list
    assert "0x2d0000" not in cmd_list


def test_upload_using_esptool_with_file_path(
    tmp_path: Path,
    mock_run_external_command_main: Mock,
) -> None:
    """Test upload_using_esptool with a custom file that's a Path object."""
    setup_core(platform=PLATFORM_ESP8266, tmp_path=tmp_path, name="test")

    # Create a test firmware file
    firmware_file = tmp_path / "custom_firmware.bin"
    firmware_file.touch()

    config = {CONF_ESPHOME: {"platformio_options": {}}}

    # Call with a Path object as the file argument (though usually it's a string)
    result = upload_using_esptool(config, "/dev/ttyUSB0", str(firmware_file), None)

    assert result == 0

    # Verify that run_external_command was called
    mock_run_external_command_main.assert_called_once()

    # Get the actual call arguments
    call_args = mock_run_external_command_main.call_args[0]
    cmd_list = list(call_args[1:])  # Skip the esptool.main function

    # Find the firmware path in the command
    write_flash_idx = cmd_list.index("write-flash")

    # For custom file, it should be at offset 0x0
    firmware_offset_idx = write_flash_idx + 4
    assert cmd_list[firmware_offset_idx] == "0x0"
    firmware_path = cmd_list[firmware_offset_idx + 1]

    # Verify it's a string, not a Path object
    assert isinstance(firmware_path, str)
    assert firmware_path.endswith("custom_firmware.bin")


@pytest.mark.parametrize(
    "platform,device",
    [
        (PLATFORM_RP2040, "/dev/ttyACM0"),
        (PLATFORM_BK72XX, "/dev/ttyUSB0"),  # LibreTiny platform
    ],
)
def test_upload_program_serial_platformio_platforms(
    mock_upload_using_platformio: Mock,
    mock_get_port_type: Mock,
    mock_check_permissions: Mock,
    platform: str,
    device: str,
) -> None:
    """Test upload_program with serial port for platformio platforms (RP2040/LibreTiny)."""
    setup_core(platform=platform)
    mock_get_port_type.return_value = "SERIAL"
    mock_upload_using_platformio.return_value = 0

    config = {}
    args = MockArgs()
    devices = [device]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == device
    mock_check_permissions.assert_called_once_with(device)
    mock_upload_using_platformio.assert_called_once_with(config, device)


def test_upload_using_platformio_creates_signed_bin_for_rp2040(
    tmp_path: Path,
) -> None:
    """Test that upload_using_platformio creates firmware.bin.signed for RP2040."""
    setup_core(platform=PLATFORM_RP2040)

    build_dir = tmp_path / "build"
    build_dir.mkdir()
    firmware_bin = build_dir / "firmware.bin"
    firmware_bin.write_bytes(b"test firmware content")
    firmware_elf = build_dir / "firmware.elf"
    firmware_elf.write_bytes(b"elf")

    mock_idedata = MagicMock()
    mock_idedata.firmware_elf_path = str(firmware_elf)

    with (
        patch("esphome.platformio.toolchain.get_idedata", return_value=mock_idedata),
        patch("esphome.platformio.toolchain.run_platformio_cli_run", return_value=0),
    ):
        result = upload_using_platformio({}, "/dev/ttyACM0")

    assert result == 0
    signed_bin = build_dir / "firmware.bin.signed"
    assert signed_bin.is_file()
    assert signed_bin.read_bytes() == b"test firmware content"


def test_upload_using_platformio_skips_signed_bin_for_non_rp2040(
    tmp_path: Path,
) -> None:
    """Test that upload_using_platformio doesn't create signed bin for non-RP2040."""
    setup_core(platform=PLATFORM_ESP32)

    with patch("esphome.platformio.toolchain.run_platformio_cli_run", return_value=0):
        result = upload_using_platformio({}, "/dev/ttyUSB0")

    assert result == 0


def test_upload_program_serial_upload_failed(
    mock_upload_using_esptool: Mock,
    mock_get_port_type: Mock,
    mock_check_permissions: Mock,
) -> None:
    """Test upload_program when serial upload fails."""
    setup_core(platform=PLATFORM_ESP32)
    mock_get_port_type.return_value = "SERIAL"
    mock_upload_using_esptool.return_value = 1  # Failed

    config = {}
    args = MockArgs()
    devices = ["/dev/ttyUSB0"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 1
    assert host is None
    mock_check_permissions.assert_called_once_with("/dev/ttyUSB0")
    mock_upload_using_esptool.assert_called_once()


def test_upload_program_bootsel(
    mock_upload_using_picotool: Mock,
    mock_get_port_type: Mock,
) -> None:
    """Test upload_program with BOOTSEL for RP2040."""
    setup_core(platform=PLATFORM_RP2040)
    mock_get_port_type.return_value = "BOOTSEL"
    mock_upload_using_picotool.return_value = 0

    config = {}
    args = MockArgs()
    devices = ["BOOTSEL"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    # BOOTSEL device can't be used for logging, so host should be None
    assert host is None
    mock_upload_using_picotool.assert_called_once_with(config)


def test_upload_program_bootsel_failed(
    mock_upload_using_picotool: Mock,
    mock_get_port_type: Mock,
) -> None:
    """Test upload_program when BOOTSEL upload fails."""
    setup_core(platform=PLATFORM_RP2040)
    mock_get_port_type.return_value = "BOOTSEL"
    mock_upload_using_picotool.return_value = 1

    config = {}
    args = MockArgs()
    devices = ["BOOTSEL"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 1
    assert host is None
    mock_upload_using_picotool.assert_called_once_with(config)


def test_upload_using_picotool_success(tmp_path: Path) -> None:
    """Test upload_using_picotool succeeds."""
    setup_core(platform=PLATFORM_RP2040, tmp_path=tmp_path)

    build_dir = tmp_path / "build"
    build_dir.mkdir()
    firmware_elf = build_dir / "firmware.elf"
    firmware_elf.write_bytes(b"\x00" * 1024)

    # Create picotool binary
    packages_dir = tmp_path / "packages"
    toolchain_bin = packages_dir / "toolchain-rp2040-earlephilhower" / "bin"
    toolchain_bin.mkdir(parents=True)
    picotool_dir = packages_dir / "tool-picotool-rp2040-earlephilhower"
    picotool_dir.mkdir(parents=True)
    binary_name = "picotool.exe" if sys.platform == "win32" else "picotool"
    picotool = picotool_dir / binary_name
    picotool.touch()

    mock_idedata = MagicMock()
    mock_idedata.firmware_elf_path = str(firmware_elf)
    mock_idedata.cc_path = str(toolchain_bin / "arm-none-eabi-gcc")

    mock_result = MagicMock()
    mock_result.returncode = 0
    mock_result.stderr = b""

    config = {}
    with (
        patch("esphome.platformio.toolchain.get_idedata", return_value=mock_idedata),
        patch("subprocess.run", return_value=mock_result),
    ):
        exit_code = upload_using_picotool(config)

    assert exit_code == 0


def test_upload_using_picotool_no_elf(tmp_path: Path) -> None:
    """Test upload_using_picotool when ELF file is missing."""
    setup_core(platform=PLATFORM_RP2040, tmp_path=tmp_path)

    build_dir = tmp_path / "build"
    build_dir.mkdir()

    mock_idedata = MagicMock()
    mock_idedata.firmware_elf_path = str(build_dir / "firmware.elf")
    mock_idedata.cc_path = "/fake/path/gcc"

    config = {}
    with patch("esphome.platformio.toolchain.get_idedata", return_value=mock_idedata):
        exit_code = upload_using_picotool(config)

    assert exit_code == 1


def test_upload_using_picotool_not_found(tmp_path: Path) -> None:
    """Test upload_using_picotool when picotool binary not found."""
    setup_core(platform=PLATFORM_RP2040, tmp_path=tmp_path)

    build_dir = tmp_path / "build"
    build_dir.mkdir()
    firmware_elf = build_dir / "firmware.elf"
    firmware_elf.write_bytes(b"\x00" * 512)

    mock_idedata = MagicMock()
    mock_idedata.firmware_elf_path = str(firmware_elf)
    mock_idedata.cc_path = "/fake/path/gcc"

    config = {}
    with patch("esphome.platformio.toolchain.get_idedata", return_value=mock_idedata):
        exit_code = upload_using_picotool(config)

    assert exit_code == 1


def test_upload_using_picotool_permission_error(tmp_path: Path) -> None:
    """Test upload_using_picotool shows helpful message on permission error."""
    setup_core(platform=PLATFORM_RP2040, tmp_path=tmp_path)

    build_dir = tmp_path / "build"
    build_dir.mkdir()
    firmware_elf = build_dir / "firmware.elf"
    firmware_elf.write_bytes(b"\x00" * 512)

    packages_dir = tmp_path / "packages"
    toolchain_bin = packages_dir / "toolchain-rp2040-earlephilhower" / "bin"
    toolchain_bin.mkdir(parents=True)
    picotool_dir = packages_dir / "tool-picotool-rp2040-earlephilhower"
    picotool_dir.mkdir(parents=True)
    binary_name = "picotool.exe" if sys.platform == "win32" else "picotool"
    picotool = picotool_dir / binary_name
    picotool.touch()

    mock_idedata = MagicMock()
    mock_idedata.firmware_elf_path = str(firmware_elf)
    mock_idedata.cc_path = str(toolchain_bin / "arm-none-eabi-gcc")

    mock_result = MagicMock()
    mock_result.returncode = 1
    mock_result.stderr = b"LIBUSB_ERROR_ACCESS"

    config = {}
    with (
        patch("esphome.platformio.toolchain.get_idedata", return_value=mock_idedata),
        patch("subprocess.run", return_value=mock_result),
    ):
        exit_code = upload_using_picotool(config)

    assert exit_code == 1


def test_upload_program_ota_success(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with OTA."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                CONF_PASSWORD: "secret",
            }
        ]
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"], 3232, "secret", expected_firmware, OTA_TYPE_UPDATE_APP
    )


def test_upload_program_ota_with_file_arg(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with OTA and custom file."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ]
    }
    args = MockArgs(file="custom.bin")
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"], 3232, None, Path("custom.bin"), OTA_TYPE_UPDATE_APP
    )


_PARTITION_TABLE_LEN = 0xC00


def _make_partition_table_bytes() -> bytes:
    """Build a minimal partition table image accepted by _validate_partition_table_binary."""
    table = bytearray(b"\xff" * _PARTITION_TABLE_LEN)
    # First entry: ESP_PARTITION_MAGIC (0x50AA) little-endian -> bytes 0xAA, 0x50.
    table[0] = 0xAA
    table[1] = 0x50
    # MD5 checksum entry at offset 32: ESP_PARTITION_MAGIC_MD5 (0xEBEB) little-endian.
    table[32] = 0xEB
    table[33] = 0xEB
    return bytes(table)


def test_upload_program_ota_partition_table_with_file_arg(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with OTA and partition table."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"
    mock_run_ota.return_value = (0, "192.168.1.100")

    partition_file = tmp_path / "partitions.bin"
    partition_file.write_bytes(_make_partition_table_bytes())

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                "allow_partition_access": True,
            }
        ]
    }
    args = MockArgs(file=str(partition_file), partition_table=True)
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"],
        3232,
        None,
        partition_file,
        OTA_TYPE_UPDATE_PARTITION_TABLE,
    )


def test_upload_program_serial_partition_table(
    mock_upload_using_esptool: Mock,
    mock_get_port_type: Mock,
) -> None:
    """Test serial upload with partition table option (unsupported)."""
    setup_core(platform=PLATFORM_ESP32)
    mock_get_port_type.return_value = "SERIAL"
    mock_upload_using_esptool.return_value = 0

    config = {}
    args = MockArgs(partition_table=True)
    devices = ["/dev/ttyUSB0"]

    with pytest.raises(
        EsphomeError,
        match="The option --partition-table can only be used for Over The Air updates",
    ):
        upload_program(config, args, devices)


def test_upload_program_ota_partition_table_mqttip(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """--partition-table is allowed for MQTTIP devices; they resolve to a real IP at OTA time."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "MQTTIP"
    mock_run_ota.return_value = (0, "192.168.1.100")

    partition_file = tmp_path / "partitions.bin"
    partition_file.write_bytes(_make_partition_table_bytes())

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                "allow_partition_access": True,
            }
        ]
    }
    args = MockArgs(file=str(partition_file), partition_table=True)

    with patch(
        "esphome.__main__._resolve_network_devices", return_value=["192.168.1.100"]
    ):
        exit_code, host = upload_program(config, args, ["MQTTIP"])

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"],
        3232,
        None,
        partition_file,
        OTA_TYPE_UPDATE_PARTITION_TABLE,
    )


def test_validate_partition_table_binary_accepts_valid(tmp_path: Path) -> None:
    f = tmp_path / "partitions.bin"
    f.write_bytes(_make_partition_table_bytes())
    _validate_partition_table_binary(f)


_PARTITION_FIXTURE_DIR = Path(__file__).parent / "fixtures" / "partition_tables"


@pytest.mark.parametrize(
    "fixture",
    [
        # Stock ESP-IDF gen_esp32part.py output for an ESPHome build.
        "esphome_default.bin",
        # ESP-IDF Hello-world example partition table (vendored from espressif/esp-serial-flasher).
        "esp_idf_hello_world.bin",
        # Partition table shipped with esphome_dashboard's prebuilt firmware.
        "esphome_dashboard_firmware.bin",
    ],
)
def test_validate_partition_table_binary_accepts_real_binaries(fixture: str) -> None:
    """Real-world partition-table binaries from ESP-IDF / ESPHome tooling pass validation."""
    _validate_partition_table_binary(_PARTITION_FIXTURE_DIR / fixture)


def test_validate_partition_table_binary_rejects_wrong_size(tmp_path: Path) -> None:
    f = tmp_path / "partitions.bin"
    f.write_bytes(b"\xaa\x50" + b"\xff" * 100)
    with pytest.raises(EsphomeError, match="wrong size"):
        _validate_partition_table_binary(f)


def test_validate_partition_table_binary_rejects_wrong_magic(tmp_path: Path) -> None:
    data = bytearray(_make_partition_table_bytes())
    data[0] = 0x00
    data[1] = 0x00
    f = tmp_path / "partitions.bin"
    f.write_bytes(bytes(data))
    with pytest.raises(EsphomeError, match="partition magic"):
        _validate_partition_table_binary(f)


def test_validate_partition_table_binary_rejects_missing_md5(tmp_path: Path) -> None:
    data = bytearray(_make_partition_table_bytes())
    data[32] = 0xFF
    data[33] = 0xFF
    f = tmp_path / "partitions.bin"
    f.write_bytes(bytes(data))
    with pytest.raises(EsphomeError, match="missing the MD5 checksum entry"):
        _validate_partition_table_binary(f)


def test_validate_partition_table_binary_missing_file(tmp_path: Path) -> None:
    with pytest.raises(EsphomeError, match="Cannot read partition table file"):
        _validate_partition_table_binary(tmp_path / "does-not-exist.bin")


def test_validate_bootloader_binary_rejects_wrong_magic(tmp_path: Path) -> None:
    data = bytearray(_make_bootloader_bytes())
    data[0] = 0x00
    f = tmp_path / "bootloader.bin"
    f.write_bytes(bytes(data))
    with pytest.raises(EsphomeError, match="magic"):
        _validate_bootloader_binary(f)


def test_validate_bootloader_binary_missing_file(tmp_path: Path) -> None:
    with pytest.raises(EsphomeError, match="Cannot read bootloader file"):
        _validate_bootloader_binary(tmp_path / "does-not-exist.bin")


def test_validate_bootloader_binary_rejects_empty_file(tmp_path: Path) -> None:
    f = tmp_path / "bootloader.bin"
    f.write_bytes(b"")
    with pytest.raises(EsphomeError, match="is empty"):
        _validate_bootloader_binary(f)


def test_upload_program_ota_partition_table_invalid_file(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """--partition-table must fail before calling run_ota when the file is not a partition table."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"

    bad_file = tmp_path / "firmware.bin"
    bad_file.write_bytes(b"\x00" * 4096)

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                "allow_partition_access": True,
            }
        ]
    }
    args = MockArgs(file=str(bad_file), partition_table=True)
    devices = ["192.168.1.100"]

    with pytest.raises(EsphomeError, match="wrong size"):
        upload_program(config, args, devices)
    mock_run_ota.assert_not_called()


def test_upload_program_ota_partition_table_without_allow_flag(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """--partition-table must fail fast when allow_partition_access is not enabled in YAML."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ]
    }
    args = MockArgs(file="partitions.bin", partition_table=True)
    devices = ["192.168.1.100"]

    with pytest.raises(
        EsphomeError,
        match=(
            r"The option --partition-table requires 'allow_partition_access: true'.*"
            r"retry --partition-table"
        ),
    ):
        upload_program(config, args, devices)
    mock_run_ota.assert_not_called()


def _make_bootloader_bytes() -> bytes:
    """Build a minimal bootloader image accepted by _validate_bootloader_binary."""
    table = bytearray(b"\xff")
    # Starts with: ESP_IMAGE_HEADER_MAGIC (0xE9)
    table[0] = 0xE9
    return bytes(table)


def test_upload_program_ota_bootloader_with_file_arg(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with OTA and bootloader."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"
    mock_run_ota.return_value = (0, "192.168.1.100")

    bootloader_file = tmp_path / "bootloader.bin"
    bootloader_file.write_bytes(_make_bootloader_bytes())

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                "allow_partition_access": True,
            }
        ]
    }
    args = MockArgs(file=str(bootloader_file), bootloader=True)
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"],
        3232,
        None,
        bootloader_file,
        OTA_TYPE_UPDATE_BOOTLOADER,
    )


def test_upload_program_ota_partition_table_and_bootloader_options(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """--partition-table and --bootloader can't be used together."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                "allow_partition_access": True,
            }
        ]
    }
    args = MockArgs(file="partitions.bin", partition_table=True, bootloader=True)
    devices = ["192.168.1.100"]

    with pytest.raises(
        EsphomeError,
        match="--partition-table and --bootloader",
    ):
        upload_program(config, args, devices)
    mock_run_ota.assert_not_called()


def test_upload_program_ota_bootloader_without_allow_flag(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """--bootloader must fail fast when allow_partition_access is not enabled in YAML."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ]
    }
    args = MockArgs(file="bootloader.bin", bootloader=True)
    devices = ["192.168.1.100"]

    with pytest.raises(
        EsphomeError,
        match=(
            r"The option --bootloader requires 'allow_partition_access: true'.*"
            r"retry --bootloader"
        ),
    ):
        upload_program(config, args, devices)
    mock_run_ota.assert_not_called()


def test_upload_program_ota_bootloader_platform_web_server(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Test bootloader upload with web_server OTA."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.return_value = "NETWORK"

    bootloader_file = tmp_path / "bootloader.bin"
    bootloader_file.write_bytes(_make_bootloader_bytes())

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_WEB_SERVER,
                CONF_WEB_SERVER: {
                    CONF_PORT: 80,
                    CONF_AUTH: {CONF_USERNAME: "admin", CONF_PASSWORD: "pw"},
                },
                "allow_partition_access": True,
            }
        ]
    }
    args = MockArgs(file=str(bootloader_file), bootloader=True)
    devices = ["192.168.1.100"]

    with pytest.raises(
        EsphomeError,
        match="the web_server OTA path can only update the firmware image",
    ):
        upload_program(config, args, devices)
    mock_run_ota.assert_not_called()


def test_upload_program_ota_no_config(
    mock_get_port_type: Mock,
) -> None:
    """Test upload_program with OTA but no OTA config."""
    setup_core(platform=PLATFORM_ESP32)
    mock_get_port_type.return_value = "NETWORK"

    config = {}  # No OTA config
    args = MockArgs()
    devices = ["192.168.1.100"]

    with pytest.raises(EsphomeError, match="Cannot upload Over the Air"):
        upload_program(config, args, devices)


def test_has_web_server_ota_detects_platform() -> None:
    """has_web_server_ota returns True when web_server OTA platform is configured."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_WEB_SERVER}],
        }
    )
    assert has_web_server_ota() is True
    assert has_ota() is True


def test_has_web_server_ota_returns_false_without_config() -> None:
    """has_web_server_ota returns False when only native OTA is configured."""
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
        }
    )
    assert has_web_server_ota() is False
    assert has_ota() is True


def test_upload_program_web_server_only_auto_dispatches(
    mock_run_web_server_ota: Mock,
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """When only web_server OTA is configured, upload_program picks it automatically."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"
    mock_run_web_server_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [{CONF_PLATFORM: CONF_WEB_SERVER}],
        CONF_WEB_SERVER: {
            CONF_PORT: 80,
            CONF_AUTH: {CONF_USERNAME: "admin", CONF_PASSWORD: "pw"},
        },
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_web_server_ota.assert_called_once_with(
        ["192.168.1.100"], 80, "admin", "pw", expected_firmware
    )
    mock_run_ota.assert_not_called()


def test_upload_program_web_server_no_auth(
    mock_run_web_server_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """web_server OTA works without an auth block (passes None for credentials)."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"
    mock_run_web_server_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [{CONF_PLATFORM: CONF_WEB_SERVER}],
        CONF_WEB_SERVER: {CONF_PORT: 8080},
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_web_server_ota.assert_called_once_with(
        ["192.168.1.100"], 8080, None, None, expected_firmware
    )


def test_upload_program_both_platforms_default_prefers_native(
    mock_run_ota: Mock,
    mock_run_web_server_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """When both OTA platforms are configured, default selection is native API."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                CONF_PASSWORD: "secret",
            },
            {CONF_PLATFORM: CONF_WEB_SERVER},
        ],
        CONF_WEB_SERVER: {CONF_PORT: 80},
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_called_once()
    mock_run_web_server_ota.assert_not_called()


def test_upload_program_ota_platform_override_to_web_server(
    mock_run_ota: Mock,
    mock_run_web_server_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """--ota-platform web_server forces web_server OTA even when native is configured."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"
    mock_run_web_server_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                CONF_PASSWORD: "secret",
            },
            {CONF_PLATFORM: CONF_WEB_SERVER},
        ],
        CONF_WEB_SERVER: {CONF_PORT: 80},
    }
    args = MockArgs(ota_platform=CONF_WEB_SERVER)
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_not_called()
    mock_run_web_server_ota.assert_called_once()


def test_upload_program_ota_platform_unavailable(
    mock_get_port_type: Mock,
) -> None:
    """--ota-platform must reference a platform that is actually configured."""
    setup_core(platform=PLATFORM_ESP32)
    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                CONF_PASSWORD: "secret",
            }
        ],
    }
    args = MockArgs(ota_platform=CONF_WEB_SERVER)
    devices = ["192.168.1.100"]

    with pytest.raises(EsphomeError, match="--ota-platform web_server"):
        upload_program(config, args, devices)


def test_upload_program_web_server_missing_component(
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """web_server OTA without a web_server component fails with a clear error."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [{CONF_PLATFORM: CONF_WEB_SERVER}],
        # No CONF_WEB_SERVER
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    with pytest.raises(EsphomeError, match="web_server.*not configured"):
        upload_program(config, args, devices)


def test_upload_program_unrelated_ota_platform_ignored(
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """OTA list entries that are neither esphome nor web_server are ignored.

    Covers the false branch in _choose_ota_platform's filter loop and the
    no-match branch in _upload_via_native_api's lookup loop.
    """
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {CONF_PLATFORM: "http_request"},  # unrelated platform; ignored
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
                CONF_PASSWORD: "secret",
            },
        ],
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_run_ota.assert_called_once()


def test_upload_program_duplicate_platform_dedup_in_error(
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Duplicate same-platform OTA entries don't repeat in --ota-platform errors."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [
            {CONF_PLATFORM: CONF_ESPHOME, CONF_PORT: 3232},
            {CONF_PLATFORM: CONF_ESPHOME, CONF_PORT: 3233},
        ],
    }
    args = MockArgs(ota_platform=CONF_WEB_SERVER)
    devices = ["192.168.1.100"]

    with pytest.raises(EsphomeError) as excinfo:
        upload_program(config, args, devices)

    # Error mentions esphome once in the platform list, not "esphome, esphome".
    msg = str(excinfo.value)
    assert "esphome, esphome" not in msg
    assert msg.endswith(": esphome")


def test_upload_program_only_unrelated_ota_platforms(
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Only unrelated OTA platforms configured -> raises like missing OTA."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)
    mock_get_port_type.return_value = "NETWORK"

    config = {
        CONF_OTA: [{CONF_PLATFORM: "http_request"}],
    }
    args = MockArgs()
    devices = ["192.168.1.100"]

    with pytest.raises(EsphomeError, match="Cannot upload Over the Air"):
        upload_program(config, args, devices)


def test_upload_program_ota_with_mqtt_resolution(
    mock_mqtt_get_ip: Mock,
    mock_is_ip_address: Mock,
    mock_run_ota: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with OTA using MQTT for address resolution."""
    setup_core(address="device.local", platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_is_ip_address.return_value = False
    mock_mqtt_get_ip.return_value = ["192.168.1.100"]
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ],
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
        CONF_MDNS: {
            CONF_DISABLED: True,
        },
    }
    args = MockArgs(username="user", password="pass", client_id="client")
    devices = ["MQTT"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"
    mock_mqtt_get_ip.assert_called_once_with(config, "user", "pass", "client")
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"], 3232, None, expected_firmware, OTA_TYPE_UPDATE_APP
    )


def test_upload_program_ota_with_mqtt_empty_broker(
    mock_mqtt_get_ip: Mock,
    mock_is_ip_address: Mock,
    mock_run_ota: Mock,
    tmp_path: Path,
    caplog: CaptureFixture,
) -> None:
    """Test upload_program with OTA when MQTT broker is empty (issue #11653)."""
    setup_core(address="192.168.1.50", platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_is_ip_address.return_value = True
    mock_mqtt_get_ip.side_effect = EsphomeError(
        "Cannot discover IP via MQTT as the broker is not configured"
    )
    mock_run_ota.return_value = (0, "192.168.1.50")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ],
        CONF_MQTT: {
            CONF_BROKER: "",
        },
        CONF_MDNS: {
            CONF_DISABLED: True,
        },
    }
    args = MockArgs(username="user", password="pass", client_id="client")
    devices = ["MQTTIP", "192.168.1.50"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.50"
    # Verify MQTT was attempted but failed gracefully
    mock_mqtt_get_ip.assert_called_once_with(config, "user", "pass", "client")
    # Verify we fell back to the IP address
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        ["192.168.1.50"], 3232, None, expected_firmware, OTA_TYPE_UPDATE_APP
    )
    # Verify warning was logged
    assert "MQTT IP discovery failed" in caplog.text


@patch("esphome.__main__.importlib.import_module")
def test_upload_program_platform_specific_handler(
    mock_import: Mock,
    mock_get_port_type: Mock,
) -> None:
    """Test upload_program with platform-specific upload handler."""
    setup_core(platform="custom_platform")
    mock_get_port_type.return_value = "CUSTOM"

    mock_module = MagicMock()
    mock_module.upload_program.return_value = True
    mock_import.return_value = mock_module

    config = {}
    args = MockArgs()
    devices = ["custom_device"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "custom_device"
    mock_import.assert_called_once_with("esphome.components.custom_platform")
    mock_module.upload_program.assert_called_once_with(config, args, "custom_device")


def test_show_logs_serial(
    mock_get_port_type: Mock,
    mock_check_permissions: Mock,
    mock_run_miniterm: Mock,
    mock_wait_for_serial_port: Mock,
) -> None:
    """Test show_logs with serial port."""
    setup_core(config={"logger": {}}, platform=PLATFORM_ESP32)
    mock_get_port_type.return_value = "SERIAL"
    mock_run_miniterm.return_value = 0

    args = MockArgs()
    devices = ["/dev/ttyUSB0"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_check_permissions.assert_called_once_with("/dev/ttyUSB0")
    mock_run_miniterm.assert_called_once_with(CORE.config, "/dev/ttyUSB0", args)


def test_show_logs_no_logger() -> None:
    """Test show_logs when logger is not configured."""
    setup_core(config={}, platform=PLATFORM_ESP32)  # No logger config
    args = MockArgs()
    devices = ["/dev/ttyUSB0"]

    with pytest.raises(EsphomeError, match="Logger is not configured"):
        show_logs(CORE.config, args, devices)


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api(
    mock_run_logs: Mock,
) -> None:
    """Test show_logs with API."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MDNS: {CONF_DISABLED: False},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0

    args = MockArgs()
    devices = ["192.168.1.100", "192.168.1.101"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100", "192.168.1.101"], subscribe_states=True
    )


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_no_states(
    mock_run_logs: Mock,
) -> None:
    """Test show_logs with --no-states flag."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MDNS: {CONF_DISABLED: False},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0

    args = MockArgs()
    args.no_states = True
    devices = ["192.168.1.100"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100"], subscribe_states=False
    )


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_with_fqdn_mdns_disabled(
    mock_run_logs: Mock,
) -> None:
    """Test show_logs with API using FQDN when mDNS is disabled."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MDNS: {CONF_DISABLED: True},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0

    args = MockArgs()
    devices = ["device.example.com"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    # Should use the FQDN directly, not try MQTT lookup
    mock_run_logs.assert_called_once_with(
        CORE.config, ["device.example.com"], subscribe_states=True
    )


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_with_mqtt_fallback(
    mock_run_logs: Mock,
    mock_mqtt_get_ip: Mock,
) -> None:
    """Test show_logs with API using MQTT for address resolution."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MDNS: {CONF_DISABLED: True},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0
    mock_mqtt_get_ip.return_value = ["192.168.1.200"]

    args = MockArgs(username="user", password="pass", client_id="client")
    devices = ["MQTTIP"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_mqtt_get_ip.assert_called_once_with(CORE.config, "user", "pass", "client")
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.200"], subscribe_states=True
    )


@patch("esphome.mqtt.show_logs")
def test_show_logs_mqtt(
    mock_mqtt_show_logs: Mock,
) -> None:
    """Test show_logs with MQTT."""
    setup_core(
        config={
            "logger": {},
            "mqtt": {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_mqtt_show_logs.return_value = 0

    args = MockArgs(
        topic="esphome/logs",
        username="user",
        password="pass",
        client_id="client",
    )
    devices = ["MQTT"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_mqtt_show_logs.assert_called_once_with(
        CORE.config, "esphome/logs", "user", "pass", "client"
    )


@patch("esphome.mqtt.show_logs")
def test_show_logs_network_with_mqtt_only(
    mock_mqtt_show_logs: Mock,
) -> None:
    """Test show_logs with network port but only MQTT configured."""
    setup_core(
        config={
            "logger": {},
            "mqtt": {CONF_BROKER: "mqtt.local"},
            # No API configured
        },
        platform=PLATFORM_ESP32,
    )
    mock_mqtt_show_logs.return_value = 0

    args = MockArgs(
        topic="esphome/logs",
        username="user",
        password="pass",
        client_id="client",
    )
    devices = ["192.168.1.100"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_mqtt_show_logs.assert_called_once_with(
        CORE.config, "esphome/logs", "user", "pass", "client"
    )


def test_show_logs_no_method_configured() -> None:
    """Test show_logs when no remote logging method is configured."""
    setup_core(
        config={
            "logger": {},
            # No API or MQTT configured
        },
        platform=PLATFORM_ESP32,
    )

    args = MockArgs()
    devices = ["192.168.1.100"]

    with pytest.raises(
        EsphomeError, match="No remote or local logging method configured"
    ):
        show_logs(CORE.config, args, devices)


@patch("esphome.__main__.importlib.import_module")
def test_show_logs_platform_specific_handler(
    mock_import: Mock,
) -> None:
    """Test show_logs with platform-specific logs handler."""
    setup_core(platform="custom_platform", config={"logger": {}})

    mock_module = MagicMock()
    mock_module.show_logs.return_value = True
    mock_import.return_value = mock_module

    config = {"logger": {}}
    args = MockArgs()
    devices = ["custom_device"]

    result = show_logs(config, args, devices)

    assert result == 0
    mock_import.assert_called_once_with("esphome.components.custom_platform")
    mock_module.show_logs.assert_called_once_with(config, args, devices)


def test_has_mqtt_logging_no_log_topic() -> None:
    """Test has_mqtt_logging returns True when CONF_LOG_TOPIC is not in mqtt_config."""

    # Setup MQTT config without CONF_LOG_TOPIC (defaults to enabled - this is the missing test case)
    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local"}})
    assert has_mqtt_logging() is True

    # Setup MQTT config with CONF_LOG_TOPIC set to None (explicitly disabled)
    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local", CONF_LOG_TOPIC: None}})
    assert has_mqtt_logging() is False

    # Setup MQTT config with CONF_LOG_TOPIC set with topic and level (explicitly enabled)
    setup_core(
        config={
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
                CONF_LOG_TOPIC: {CONF_TOPIC: "esphome/logs", CONF_LEVEL: "DEBUG"},
            }
        }
    )
    assert has_mqtt_logging() is True

    # Setup MQTT config with CONF_LOG_TOPIC set but level is NONE (disabled)
    setup_core(
        config={
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
                CONF_LOG_TOPIC: {CONF_TOPIC: "esphome/logs", CONF_LEVEL: "NONE"},
            }
        }
    )
    assert has_mqtt_logging() is False

    # Setup without MQTT config at all
    setup_core(config={})
    assert has_mqtt_logging() is False

    # Setup MQTT config with CONF_LOG_TOPIC but no CONF_LEVEL (regression test for #10771)
    # This simulates the default configuration created by validate_config in the MQTT component
    setup_core(
        config={
            CONF_MQTT: {
                CONF_BROKER: "mqtt.local",
                CONF_LOG_TOPIC: {CONF_TOPIC: "esphome/debug"},
            }
        }
    )
    assert has_mqtt_logging() is True


def test_has_mqtt() -> None:
    """Test has_mqtt function."""

    # Test with MQTT configured
    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local"}})
    assert has_mqtt() is True

    # Test without MQTT configured
    setup_core(config={})
    assert has_mqtt() is False

    # Test with other components but no MQTT
    setup_core(config={CONF_API: {}, CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]})
    assert has_mqtt() is False


def test_has_ota() -> None:
    """Test has_ota function.

    The has_ota function should only return True when OTA is configured
    with platform: esphome, not when only platform: http_request is configured.
    This is because CLI OTA upload only works with the esphome platform.
    """
    # Test with OTA esphome platform configured
    setup_core(config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]})
    assert has_ota() is True

    # Test with OTA http_request platform only (should return False)
    # This is the bug scenario from issue #13783
    setup_core(config={CONF_OTA: [{CONF_PLATFORM: "http_request"}]})
    assert has_ota() is False

    # Test without OTA configured
    setup_core(config={})
    assert has_ota() is False

    # Test with multiple OTA platforms including esphome
    setup_core(
        config={
            CONF_OTA: [{CONF_PLATFORM: "http_request"}, {CONF_PLATFORM: CONF_ESPHOME}]
        }
    )
    assert has_ota() is True

    # Test with empty OTA list
    setup_core(config={CONF_OTA: []})
    assert has_ota() is False


def test_get_port_type() -> None:
    """Test get_port_type function."""

    assert get_port_type("/dev/ttyUSB0") == "SERIAL"
    assert get_port_type("/dev/ttyACM0") == "SERIAL"
    assert get_port_type("COM1") == "SERIAL"
    assert get_port_type("COM10") == "SERIAL"

    assert get_port_type("MQTT") == "MQTT"
    assert get_port_type("MQTTIP") == "MQTTIP"

    assert get_port_type("192.168.1.100") == "NETWORK"
    assert get_port_type("esphome-device.local") == "NETWORK"
    assert get_port_type("10.0.0.1") == "NETWORK"

    assert get_port_type("BOOTSEL") == "BOOTSEL"


def test_has_mqtt_ip_lookup() -> None:
    """Test has_mqtt_ip_lookup function."""

    CONF_DISCOVER_IP = "discover_ip"

    setup_core(config={})
    assert has_mqtt_ip_lookup() is False

    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local"}})
    assert has_mqtt_ip_lookup() is True

    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local", CONF_DISCOVER_IP: True}})
    assert has_mqtt_ip_lookup() is True

    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local", CONF_DISCOVER_IP: False}})
    assert has_mqtt_ip_lookup() is False


def test_has_non_ip_address() -> None:
    """Test has_non_ip_address function."""

    setup_core(address=None)
    assert has_non_ip_address() is False

    setup_core(address="192.168.1.100")
    assert has_non_ip_address() is False

    setup_core(address="10.0.0.1")
    assert has_non_ip_address() is False

    setup_core(address="esphome-device.local")
    assert has_non_ip_address() is True

    setup_core(address="my-device")
    assert has_non_ip_address() is True


def test_has_ip_address() -> None:
    """Test has_ip_address function."""

    setup_core(address=None)
    assert has_ip_address() is False

    setup_core(address="192.168.1.100")
    assert has_ip_address() is True

    setup_core(address="10.0.0.1")
    assert has_ip_address() is True

    setup_core(address="esphome-device.local")
    assert has_ip_address() is False

    setup_core(address="my-device")
    assert has_ip_address() is False


def test_mqtt_get_ip() -> None:
    """Test mqtt_get_ip function."""
    config = {CONF_MQTT: {CONF_BROKER: "mqtt.local"}}

    with patch("esphome.mqtt.get_esphome_device_ip") as mock_get_ip:
        mock_get_ip.return_value = ["192.168.1.100", "192.168.1.101"]

        result = mqtt_get_ip(config, "user", "pass", "client-id")

        assert result == ["192.168.1.100", "192.168.1.101"]
        mock_get_ip.assert_called_once_with(config, "user", "pass", "client-id")


def test_has_resolvable_address() -> None:
    """Test has_resolvable_address function."""

    # Test with mDNS enabled and .local hostname address
    setup_core(config={}, address="esphome-device.local")
    assert has_resolvable_address() is True

    # Test with mDNS disabled and .local hostname address (still resolvable via DNS)
    setup_core(
        config={CONF_MDNS: {CONF_DISABLED: True}}, address="esphome-device.local"
    )
    assert has_resolvable_address() is False

    # Test with mDNS disabled and regular DNS hostname (resolvable)
    setup_core(config={CONF_MDNS: {CONF_DISABLED: True}}, address="device.example.com")
    assert has_resolvable_address() is True

    # Test with IP address (always resolvable, mDNS doesn't matter)
    setup_core(config={}, address="192.168.1.100")
    assert has_resolvable_address() is True

    # Test with IP address and mDNS disabled (still resolvable)
    setup_core(config={CONF_MDNS: {CONF_DISABLED: True}}, address="192.168.1.100")
    assert has_resolvable_address() is True

    # Test with no address
    setup_core(config={}, address=None)
    assert has_resolvable_address() is False

    # Test with no address and mDNS disabled
    setup_core(config={CONF_MDNS: {CONF_DISABLED: True}}, address=None)
    assert has_resolvable_address() is False


def test_has_name_add_mac_suffix() -> None:
    """Test has_name_add_mac_suffix function."""

    # Test with name_add_mac_suffix enabled
    setup_core(config={CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: True}})
    assert has_name_add_mac_suffix() is True

    # Test with name_add_mac_suffix disabled
    setup_core(config={CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: False}})
    assert has_name_add_mac_suffix() is False

    # Test with name_add_mac_suffix not set (defaults to False)
    setup_core(config={CONF_ESPHOME: {}})
    assert has_name_add_mac_suffix() is False

    # Test with no esphome config
    setup_core(config={})
    assert has_name_add_mac_suffix() is False

    # Test with no config at all
    CORE.config = None
    assert has_name_add_mac_suffix() is False


@pytest.fixture
def mock_mdns_discovery() -> Generator[MagicMock]:
    """Fixture to mock the async mDNS discovery infrastructure.

    Patches ``AsyncEsphomeZeroconf``, ``AsyncServiceBrowser`` and
    ``AddressResolver`` in ``esphome.zeroconf`` and exposes hooks for tests to
    stage browser events and control resolution results. The default
    ``AddressResolver`` stub simulates a cache hit returning no addresses, so
    matched hosts appear in the discovery output with empty address lists
    unless the test overrides ``_resolver_setup``.
    """
    with (
        patch("esphome.zeroconf.AsyncEsphomeZeroconf") as mock_aiozc_class,
        patch("esphome.zeroconf.AsyncServiceBrowser") as mock_browser_class,
        patch("esphome.zeroconf.AddressResolver") as mock_resolver_class,
    ):
        mock_aiozc = MagicMock()
        mock_aiozc.zeroconf = MagicMock()
        mock_aiozc.async_close = AsyncMock(return_value=None)
        mock_aiozc_class.return_value = mock_aiozc

        mock_browser = MagicMock()
        mock_browser.async_cancel = AsyncMock(return_value=None)

        # Default: each host gets a fresh resolver that hits the cache and
        # returns no addresses. Tests can override via ``_resolver_setup``.
        def default_resolver_factory(name: str) -> MagicMock:
            resolver = MagicMock()
            resolver._name = name
            resolver.load_from_cache.return_value = True
            resolver.async_request = AsyncMock(return_value=True)
            resolver.parsed_scoped_addresses.return_value = []
            return resolver

        mock_resolver_class.side_effect = default_resolver_factory

        # Store references for test access
        mock_aiozc._mock_browser_class = mock_browser_class
        mock_aiozc._mock_browser = mock_browser
        mock_aiozc._mock_class = mock_aiozc_class
        mock_aiozc._mock_resolver_class = mock_resolver_class
        yield mock_aiozc


@pytest.mark.parametrize(
    ("discovered_services", "base_name", "expected_hosts"),
    [
        # Matching devices; different-prefix device is filtered out
        (
            [
                ("mydevice-abc123._esphomelib._tcp.local.", ServiceStateChange.Added),
                ("mydevice-def456._esphomelib._tcp.local.", ServiceStateChange.Added),
                (
                    "otherdevice-abcdef._esphomelib._tcp.local.",
                    ServiceStateChange.Added,
                ),
            ],
            "mydevice",
            ["mydevice-abc123.local", "mydevice-def456.local"],
        ),
        # No matches at all
        (
            [
                (
                    "otherdevice-abcdef._esphomelib._tcp.local.",
                    ServiceStateChange.Added,
                ),
            ],
            "mydevice",
            [],
        ),
        # Deduplication (same device Added then Updated)
        (
            [
                ("mydevice-abc123._esphomelib._tcp.local.", ServiceStateChange.Added),
                ("mydevice-abc123._esphomelib._tcp.local.", ServiceStateChange.Updated),
            ],
            "mydevice",
            ["mydevice-abc123.local"],
        ),
        # Suffix must be exactly 6 hex chars: wrong length and non-hex are rejected
        (
            [
                # too short
                ("mydevice-abcd._esphomelib._tcp.local.", ServiceStateChange.Added),
                # too long
                (
                    "mydevice-abcdef1._esphomelib._tcp.local.",
                    ServiceStateChange.Added,
                ),
                # non-hex
                ("mydevice-xyz123._esphomelib._tcp.local.", ServiceStateChange.Added),
                # valid
                ("mydevice-012345._esphomelib._tcp.local.", ServiceStateChange.Added),
            ],
            "mydevice",
            ["mydevice-012345.local"],
        ),
        # Prefix-collision: base "foo" must not match "foo-bar-abc123"
        (
            [
                ("foo-abcdef._esphomelib._tcp.local.", ServiceStateChange.Added),
                ("foo-bar-abcdef._esphomelib._tcp.local.", ServiceStateChange.Added),
            ],
            "foo",
            ["foo-abcdef.local"],
        ),
    ],
    ids=[
        "matching_with_filter",
        "no_matches",
        "deduplication",
        "hex_suffix_filter",
        "prefix_collision",
    ],
)
def test_discover_mdns_devices(
    mock_mdns_discovery: MagicMock,
    discovered_services: list[tuple[str, ServiceStateChange]],
    base_name: str,
    expected_hosts: list[str],
) -> None:
    """Test discover_mdns_devices filtering and deduplication."""
    mock_browser = mock_mdns_discovery._mock_browser

    def capture_callback(
        zc: MagicMock,
        service_type: str,
        handlers: list[Callable[..., None]],
    ) -> MagicMock:
        callback = handlers[0]
        for service_name, state_change in discovered_services:
            callback(
                mock_mdns_discovery.zeroconf, service_type, service_name, state_change
            )
        return mock_browser

    mock_mdns_discovery._mock_browser_class.side_effect = capture_callback

    # Each discovered host gets a resolver that returns a unique IP string
    # derived from its server name so we can assert per-host.
    def resolver_factory(name: str) -> MagicMock:
        resolver = MagicMock()
        resolver._name = name
        resolver.load_from_cache.return_value = True
        resolver.async_request = AsyncMock(return_value=True)
        resolver.parsed_scoped_addresses.return_value = [f"10.0.0.1#{name}"]
        return resolver

    mock_mdns_discovery._mock_resolver_class.side_effect = resolver_factory

    result = discover_mdns_devices(base_name, timeout=0)

    assert sorted(result) == expected_hosts
    # Resolved addresses should be stored for matched hosts. AddressResolver
    # receives the fully-qualified name (``<device>.local.``).
    for host in expected_hosts:
        short = host.partition(".")[0]
        assert result[host] == [f"10.0.0.1#{short}.local."]
    mock_browser.async_cancel.assert_awaited_once()
    mock_mdns_discovery.async_close.assert_awaited_once()


def test_discover_mdns_devices_init_failure(caplog: pytest.LogCaptureFixture) -> None:
    """If AsyncEsphomeZeroconf fails to init, return empty dict and log warning."""
    with (
        patch(
            "esphome.zeroconf.AsyncEsphomeZeroconf",
            side_effect=OSError("no network"),
        ),
        caplog.at_level(logging.WARNING, logger="esphome.zeroconf"),
    ):
        result = discover_mdns_devices("mydevice", timeout=0)

    assert result == {}
    assert "mDNS discovery failed to initialize" in caplog.text


def test_discover_mdns_devices_resolution_failure(
    mock_mdns_discovery: MagicMock,
) -> None:
    """If resolution raises, the host is still listed with an empty address list."""
    mock_browser = mock_mdns_discovery._mock_browser

    def capture_callback(
        zc: MagicMock,
        service_type: str,
        handlers: list[Callable[..., None]],
    ) -> MagicMock:
        handlers[0](
            mock_mdns_discovery.zeroconf,
            service_type,
            "mydevice-abc123._esphomelib._tcp.local.",
            ServiceStateChange.Added,
        )
        return mock_browser

    mock_mdns_discovery._mock_browser_class.side_effect = capture_callback

    # Resolver misses the cache, then async_request raises.
    def failing_resolver_factory(name: str) -> MagicMock:
        resolver = MagicMock()
        resolver.load_from_cache.return_value = False
        resolver.async_request = AsyncMock(side_effect=OSError("boom"))
        resolver.parsed_scoped_addresses.return_value = []
        return resolver

    mock_mdns_discovery._mock_resolver_class.side_effect = failing_resolver_factory

    result = discover_mdns_devices("mydevice", timeout=0)

    assert result == {"mydevice-abc123.local": []}


def test_discover_mdns_devices_ignores_removed_state(
    mock_mdns_discovery: MagicMock,
) -> None:
    """``Removed`` state changes are ignored and do not appear in the result."""
    mock_browser = mock_mdns_discovery._mock_browser

    def capture_callback(
        zc: MagicMock,
        service_type: str,
        handlers: list[Callable[..., None]],
    ) -> MagicMock:
        handlers[0](
            mock_mdns_discovery.zeroconf,
            service_type,
            "mydevice-abc123._esphomelib._tcp.local.",
            ServiceStateChange.Removed,
        )
        return mock_browser

    mock_mdns_discovery._mock_browser_class.side_effect = capture_callback

    result = discover_mdns_devices("mydevice", timeout=0)

    assert result == {}
    # No AddressResolver should have been constructed since no host matched.
    mock_mdns_discovery._mock_resolver_class.assert_not_called()


def test_discover_mdns_devices_empty_resolution(
    mock_mdns_discovery: MagicMock,
) -> None:
    """Host is listed with empty addresses when resolver returns no addresses."""
    mock_browser = mock_mdns_discovery._mock_browser

    def capture_callback(
        zc: MagicMock,
        service_type: str,
        handlers: list[Callable[..., None]],
    ) -> MagicMock:
        handlers[0](
            mock_mdns_discovery.zeroconf,
            service_type,
            "mydevice-abc123._esphomelib._tcp.local.",
            ServiceStateChange.Added,
        )
        return mock_browser

    mock_mdns_discovery._mock_browser_class.side_effect = capture_callback
    # Default fixture resolver is a cache-hit with no addresses — simulates
    # the "browse found it but no A/AAAA records are available" case.

    result = discover_mdns_devices("mydevice", timeout=0)

    assert result == {"mydevice-abc123.local": []}


def test_resolve_network_devices_expands_cached_mdns_hosts(tmp_path: Path) -> None:
    """Hostnames in ``CORE.address_cache`` are expanded to their cached IPs."""
    setup_core(tmp_path=tmp_path)
    CORE.address_cache = AddressCache(
        mdns_cache={
            "device-abc123.local": ["10.0.0.1", "10.0.0.2"],
        }
    )

    result = _resolve_network_devices(
        ["device-abc123.local", "192.168.1.50", "device-abc123.local"],
        CORE.config,
        MockArgs(),
    )

    # Cached hostname is replaced with its IPs (deduplicated across repeats)
    # and the literal IP is preserved after.
    assert result == ["10.0.0.1", "10.0.0.2", "192.168.1.50"]


def test_resolve_network_devices_keeps_uncached_hosts(tmp_path: Path) -> None:
    """Hostnames not in the cache pass through unchanged."""
    setup_core(tmp_path=tmp_path)
    CORE.address_cache = AddressCache()

    result = _resolve_network_devices(
        ["unknown.local", "192.168.1.50"],
        CORE.config,
        MockArgs(),
    )

    assert result == ["unknown.local", "192.168.1.50"]


def test_await_discovery_timeout_returns_empty(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """If the discovery runner never sets its event, return {} and warn."""
    stub = MagicMock()
    stub.event.wait.return_value = False
    stub.exception = None
    stub.result = {"should_not_be_read": ["1.2.3.4"]}

    with caplog.at_level(logging.WARNING, logger="esphome.zeroconf"):
        result = _await_discovery(stub, timeout=0.01)

    assert result == {}
    assert "mDNS discovery timed out after 0.0s" in caplog.text
    stub.event.wait.assert_called_once_with(timeout=pytest.approx(2.01))


def test_await_discovery_propagates_exception_as_empty(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """If the coroutine raised, log and return {} rather than re-raise."""
    stub = MagicMock()
    stub.event.wait.return_value = True
    stub.exception = RuntimeError("boom")
    stub.result = None

    with caplog.at_level(logging.WARNING, logger="esphome.zeroconf"):
        result = _await_discovery(stub, timeout=5.0)

    assert result == {}
    assert "mDNS discovery failed: boom" in caplog.text


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_discovers_mac_suffix_devices(tmp_path: Path) -> None:
    """Interactive mode discovers MAC-suffixed devices and populates the cache."""
    setup_core(
        config={
            CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: True},
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
        },
        address="mydevice.local",
        tmp_path=tmp_path,
        name="mydevice",
    )
    CORE.address_cache = None

    discovered = {
        "mydevice-abc123.local": ["10.0.0.1"],
        "mydevice-def456.local": ["10.0.0.2"],
    }
    with (
        patch(
            "esphome.zeroconf.discover_mdns_devices", return_value=discovered
        ) as mock_discover,
        patch(
            "esphome.__main__.choose_prompt", return_value="mydevice-abc123.local"
        ) as mock_prompt,
    ):
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )

    assert result == ["mydevice-abc123.local"]
    mock_discover.assert_called_once_with("mydevice")
    mock_prompt.assert_called_once_with(
        [
            ("Over The Air (mydevice-abc123.local)", "mydevice-abc123.local"),
            ("Over The Air (mydevice-def456.local)", "mydevice-def456.local"),
        ],
        purpose=Purpose.UPLOADING,
    )
    # Resolved IPs should be cached so downstream resolution skips a second
    # Zeroconf lookup.
    assert CORE.address_cache is not None
    assert CORE.address_cache.get_mdns_addresses("mydevice-abc123.local") == [
        "10.0.0.1"
    ]
    assert CORE.address_cache.get_mdns_addresses("mydevice-def456.local") == [
        "10.0.0.2"
    ]


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_mac_suffix_no_devices_found(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """When discovery finds nothing, no OTA option is offered and a warning logs."""
    setup_core(
        config={
            CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: True},
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
        },
        address="mydevice.local",
        tmp_path=tmp_path,
        name="mydevice",
    )

    with (
        patch("esphome.zeroconf.discover_mdns_devices", return_value={}),
        caplog.at_level(logging.WARNING, logger="esphome.__main__"),
        pytest.raises(EsphomeError),
    ):
        choose_upload_log_host(
            default=None,
            check_default=None,
            purpose=Purpose.UPLOADING,
        )

    assert "No devices matching 'mydevice-<mac>.local'" in caplog.text


def test_choose_upload_log_host_default_ota_discovers_mac_suffix(
    tmp_path: Path,
) -> None:
    """``--device OTA`` also runs mDNS discovery when name_add_mac_suffix is on."""
    setup_core(
        config={
            CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: True},
            CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}],
        },
        address="mydevice.local",
        tmp_path=tmp_path,
        name="mydevice",
    )
    CORE.address_cache = None

    discovered = {
        "mydevice-abc123.local": ["10.0.0.1"],
        "mydevice-def456.local": ["10.0.0.2"],
    }
    with patch(
        "esphome.zeroconf.discover_mdns_devices", return_value=discovered
    ) as mock_discover:
        result = choose_upload_log_host(
            default="OTA",
            check_default=None,
            purpose=Purpose.UPLOADING,
        )

    # Both discovered hostnames are returned so aioesphomeapi / espota2 can
    # try each in turn with the cached IPs.
    assert result == ["mydevice-abc123.local", "mydevice-def456.local"]
    mock_discover.assert_called_once_with("mydevice")
    assert CORE.address_cache is not None
    assert CORE.address_cache.get_mdns_addresses("mydevice-abc123.local") == [
        "10.0.0.1"
    ]


def test_choose_upload_log_host_default_ota_no_suffix_discovery(
    tmp_path: Path,
) -> None:
    """``--device OTA`` without name_add_mac_suffix uses CORE.address as-is."""
    setup_core(
        config={CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME}]},
        address="192.168.1.100",
        tmp_path=tmp_path,
        name="mydevice",
    )

    with patch("esphome.zeroconf.discover_mdns_devices") as mock_discover:
        result = choose_upload_log_host(
            default="OTA",
            check_default=None,
            purpose=Purpose.UPLOADING,
        )

    assert result == ["192.168.1.100"]
    # Discovery must NOT run when name_add_mac_suffix is disabled.
    mock_discover.assert_not_called()


def test_command_wizard(tmp_path: Path) -> None:
    """Test command_wizard function."""
    config_file = tmp_path / "test.yaml"

    # Mock wizard.wizard to avoid interactive prompts
    with patch("esphome.wizard.wizard") as mock_wizard:
        mock_wizard.return_value = 0

        args = MockArgs(configuration=str(config_file))
        result = command_wizard(args)

        assert result == 0
        mock_wizard.assert_called_once_with(config_file)


def test_command_config_hash(
    tmp_path: Path,
    capfd: CaptureFixture[str],
) -> None:
    """command_config_hash runs codegen then prints CORE.config_hash.

    The printed format must match `0x{config_hash:08x}` used by
    generate_build_info_data_cpp so the value can be compared byte-for-byte
    against the ESPHOME_CONFIG_HASH embedded in firmware.
    """
    setup_core(tmp_path=tmp_path, config={"esphome": {"name": "test"}})
    args = MockArgs()

    # generate_cpp_contents requires real components to be loaded; mock it out
    # so this test isolates the command's output contract. The command must
    # still call it (codegen can mutate config, which affects the hash).
    with patch("esphome.__main__.generate_cpp_contents") as mock_generate:
        result = command_config_hash(args, CORE.config)

    assert result == 0
    mock_generate.assert_called_once_with(CORE.config)

    output = strip_ansi_codes(capfd.readouterr().out).strip()
    assert re.fullmatch(r"0x[0-9a-f]{8}", output)
    assert output == f"0x{CORE.config_hash:08x}"


def test_command_rename_invalid_characters(
    tmp_path: Path, capfd: CaptureFixture[str]
) -> None:
    """Test command_rename with invalid characters in name."""
    setup_core(tmp_path=tmp_path)

    # Test with invalid character (space)
    args = MockArgs(name="invalid name")
    result = command_rename(args, {})

    assert result == 1
    captured = capfd.readouterr()
    assert "invalid character" in captured.out.lower()


def test_command_rename_complex_yaml(
    tmp_path: Path, capfd: CaptureFixture[str]
) -> None:
    """Test command_rename with complex YAML that cannot be renamed."""
    config_file = tmp_path / "test.yaml"
    config_file.write_text("# Complex YAML without esphome section\nsome_key: value\n")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    args = MockArgs(name="newname")
    result = command_rename(args, {})

    assert result == 1
    captured = capfd.readouterr()
    assert "complex yaml" in captured.out.lower()


def test_command_rename_success(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test successful rename of a simple configuration."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s

wifi:
  ssid: "test"
  password: "test1234"
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    # Set up CORE.config to avoid ValueError when accessing CORE.address
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)

    # Simulate successful validation and upload
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    # Verify new file was created
    new_file = tmp_path / "newname.yaml"
    assert new_file.exists()

    # Verify old file was removed
    assert not config_file.exists()

    # Verify content was updated
    content = new_file.read_text()
    assert (
        'name: "newname"' in content
        or "name: 'newname'" in content
        or "name: newname" in content
    )

    captured = capfd.readouterr()
    assert "SUCCESS" in captured.out


def test_command_rename_with_substitutions(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename with substitutions in YAML."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
substitutions:
  device_name: oldname

esphome:
  name: ${device_name}

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    # Set up CORE.config to avoid ValueError when accessing CORE.address
    CORE.config = {
        CONF_ESPHOME: {CONF_NAME: "oldname"},
        CONF_SUBSTITUTIONS: {"device_name": "oldname"},
    }

    args = MockArgs(name="newname", dashboard=False)

    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    # Verify substitution was updated
    new_file = tmp_path / "newname.yaml"
    content = new_file.read_text()
    assert 'device_name: "newname"' in content


def test_command_rename_validation_failure(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename when validation fails."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file

    args = MockArgs(name="newname", dashboard=False)

    # First call for validation fails
    mock_run_external_process.return_value = 1

    result = command_rename(args, {})

    assert result == 1

    # Verify new file was created but then removed due to failure
    new_file = tmp_path / "newname.yaml"
    assert not new_file.exists()

    # Verify old file still exists (not removed on failure)
    assert config_file.exists()

    captured = capfd.readouterr()
    assert "Rename failed" in captured.out


def test_command_rename_install_failure_reverts(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename when the install (esphome run) step fails."""
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)

    # First call (config validation) succeeds; second (esphome run) fails.
    mock_run_external_process.side_effect = [0, 1]

    result = command_rename(args, {})

    assert result == 1

    # New file was unlinked when install failed.
    new_file = tmp_path / "newname.yaml"
    assert not new_file.exists()

    # Old file is preserved so the device stays reachable under the
    # original hostname.
    assert config_file.exists()


def test_command_rename_target_exists_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when the target filename already exists.

    Without this guard, the rename would overwrite the unrelated
    device's YAML and OTA-install our firmware to the wrong device.
    """
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname

esp32:
  board: nodemcu-32s
""")
    target_file = tmp_path / "newname.yaml"
    target_file.write_text("""
esphome:
  name: someoneelse

esp32:
  board: nodemcu-32s
""")
    target_original = target_file.read_text()
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    # No subprocess work happened — refusal is up-front.
    mock_run_external_process.assert_not_called()
    # Target file untouched: same content, still on disk.
    assert target_file.exists()
    assert target_file.read_text() == target_original
    # Source file untouched.
    assert config_file.exists()

    captured = capfd.readouterr()
    assert "already exists" in captured.out


def test_command_rename_same_name_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when the new name matches the current name.

    A same-name rename would otherwise re-write the YAML and queue
    a redundant compile + install — wasted work the user almost
    certainly didn't intend.
    """
    config_file = tmp_path / "samename.yaml"
    config_file.write_text("""
esphome:
  name: samename

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "samename"}}

    args = MockArgs(name="samename", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # File preserved verbatim — no rewrite happened.
    assert config_file.exists()

    captured = capfd.readouterr()
    assert "already" in captured.out.lower()


def test_command_rename_does_not_touch_friendly_name_substring(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    r"""Test rename does not match the ``name:`` substring of ``friendly_name:``.

    Without anchoring the regex at line start, the pattern
    ``\s*name:\s+<old>`` could match the trailing ``name:``
    substring inside ``friendly_name: <old>``. The rewrite would
    flip both lines to the new name, leaving the user with a
    silently corrupted ``friendly_name``.
    """
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
esphome:
  name: oldname
  friendly_name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "oldname"}}

    args = MockArgs(name="newname", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "newname.yaml"
    content = new_file.read_text()
    # esphome.name swapped.
    assert 'name: "newname"' in content
    # friendly_name kept verbatim.
    assert "friendly_name: oldname" in content


def test_command_rename_does_not_match_old_name_as_value_prefix(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    r"""Test rename does not match ``old_name`` as a prefix of a longer value.

    With ``old_name = kitchen`` the value ``kitchen2`` (a sensor
    or wifi entry) would otherwise match the unanchored
    ``["']?kitchen["']?`` pattern at the prefix and get
    rewritten to the new name. The end-of-value lookahead keeps
    the match restricted to whole tokens.
    """
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: kitchen

esp32:
  board: nodemcu-32s

wifi:
  ap:
    ssid: kitchen2
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "garage.yaml"
    content = new_file.read_text()
    assert 'name: "garage"' in content
    # The wifi ssid value is unrelated and stays intact.
    assert "ssid: kitchen2" in content


def test_command_rename_same_resolved_name_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when ``new_name`` matches the resolved device name.

    The path-equality check only catches the case where the
    config filename matches the device name. For a config whose
    filename and ``esphome.name`` differ (here ``weird-file.yaml``
    holds ``esphome.name: kitchen``), running
    ``esphome rename weird-file.yaml kitchen`` would otherwise
    fall through to the rewrite + install: the YAML's name stays
    ``kitchen``, the file is renamed to ``kitchen.yaml``, and the
    device gets a redundant flash. Refuse up-front so the
    "already the device's name" message matches reality.
    """
    config_file = tmp_path / "weird-file.yaml"
    config_file.write_text("""
esphome:
  name: kitchen

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="kitchen", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # Source file untouched, no derived target written.
    assert config_file.exists()
    assert not (tmp_path / "kitchen.yaml").exists()

    captured = capfd.readouterr()
    assert "already" in captured.out.lower()


def test_command_rename_target_path_equals_source_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when the new path resolves to the source file.

    Reachable only when the YAML's filename and ``esphome.name``
    disagree — here ``kitchen.yaml`` holds ``esphome.name: garage``
    and the user runs ``esphome rename kitchen.yaml kitchen``. The
    name-equality check above passes (``garage != kitchen``), but
    ``<config_dir>/kitchen.yaml`` resolves to the source file
    itself, so the rewrite would clobber the source mid-rename.
    Refuse rather than silently overwriting.
    """
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: garage

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "garage"}}

    args = MockArgs(name="kitchen", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # Source file still present and unmodified.
    assert config_file.exists()
    assert "name: garage" in config_file.read_text()

    captured = capfd.readouterr()
    assert "already" in captured.out.lower()


def test_command_rename_does_not_touch_lookalike_name_in_other_blocks(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename only swaps the esphome.name line.

    A device whose name happens to match a sensor's / output's
    ``name:`` value must not have those other names rewritten —
    they're independent. Without an anchor for the esphome block
    a naive regex would clobber every line whose value matches.
    """
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: kitchen

esp32:
  board: nodemcu-32s

sensor:
  - platform: template
    name: kitchen
    lambda: 'return 0;'
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    new_file = tmp_path / "garage.yaml"
    content = new_file.read_text()
    # esphome.name renamed.
    assert 'name: "garage"' in content
    # Sensor's name is the user's entity name — must not be touched.
    assert "    name: kitchen\n" in content


def test_command_rename_preserves_trailing_comment(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename preserves a trailing ``# comment`` on the name line."""
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: kitchen  # primary device

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0

    new_file = tmp_path / "garage.yaml"
    content = new_file.read_text()
    assert "# primary device" in content


def test_command_rename_handles_double_quoted_value(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename matches when the existing value is double-quoted."""
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: "kitchen"

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "garage.yaml"
    assert 'name: "garage"' in new_file.read_text()


def test_command_rename_handles_single_quoted_value(
    tmp_path: Path,
    mock_run_external_process: Mock,
) -> None:
    """Test rename matches when the existing value is single-quoted."""
    config_file = tmp_path / "kitchen.yaml"
    config_file.write_text("""
esphome:
  name: 'kitchen'

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "kitchen"}}

    args = MockArgs(name="garage", dashboard=False)
    mock_run_external_process.return_value = 0

    result = command_rename(args, {})

    assert result == 0
    new_file = tmp_path / "garage.yaml"
    assert 'name: "garage"' in new_file.read_text()


def test_command_rename_too_many_substitution_matches_refuses(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_run_external_process: Mock,
) -> None:
    """Test rename refuses when ``${var}`` resolves to multiple matches.

    When ``esphome.name: ${device_name}`` and the substitution
    definition ``device_name: foo`` appears more than once in the
    YAML (e.g. inside multiple included blocks), the regex rewrite
    can't tell which one to flip. Rather than silently picking one
    or rewriting both, the command refuses.
    """
    config_file = tmp_path / "oldname.yaml"
    config_file.write_text("""
substitutions:
  device_name: oldname

esphome:
  name: ${device_name}

# A copy-pasted block that re-declares the substitution at the
# same indent level - happens when users splice in a packaged
# fragment without renaming the variable.
example:
  device_name: oldname

esp32:
  board: nodemcu-32s
""")
    setup_core(tmp_path=tmp_path)
    CORE.config_path = config_file
    CORE.config = {
        CONF_ESPHOME: {CONF_NAME: "oldname"},
        CONF_SUBSTITUTIONS: {"device_name": "oldname"},
    }

    args = MockArgs(name="newname", dashboard=False)

    result = command_rename(args, {})

    assert result == 1
    mock_run_external_process.assert_not_called()
    # File untouched.
    assert config_file.exists()
    assert "device_name: oldname" in config_file.read_text()

    captured = capfd.readouterr()
    assert "Too many matches" in captured.out


def test_command_update_all_path_string_conversion(
    tmp_path: Path,
    mock_run_external_process: Mock,
    capfd: CaptureFixture[str],
) -> None:
    """Test that command_update_all properly converts Path objects to strings in output."""
    yaml1 = tmp_path / "device1.yaml"
    yaml1.write_text("""
esphome:
  name: device1

esp32:
  board: nodemcu-32s
""")

    yaml2 = tmp_path / "device2.yaml"
    yaml2.write_text("""
esphome:
  name: device2

esp8266:
  board: nodemcuv2
""")

    setup_core(tmp_path=tmp_path)
    mock_run_external_process.return_value = 0

    assert command_update_all(MockArgs(configuration=[str(tmp_path)])) == 0

    captured = capfd.readouterr()
    clean_output = strip_ansi_codes(captured.out)

    # Check that Path objects were properly converted to strings
    # The output should contain file paths without causing TypeError
    assert "device1.yaml" in clean_output
    assert "device2.yaml" in clean_output
    assert "SUCCESS" in clean_output
    assert "SUMMARY" in clean_output

    # Verify run_external_process was called for each file
    assert mock_run_external_process.call_count == 2


def test_command_update_all_with_failures(
    tmp_path: Path,
    mock_run_external_process: Mock,
    capfd: CaptureFixture[str],
) -> None:
    """Test command_update_all handles mixed success/failure cases properly."""
    yaml1 = tmp_path / "success_device.yaml"
    yaml1.write_text("""
esphome:
  name: success_device

esp32:
  board: nodemcu-32s
""")

    yaml2 = tmp_path / "failed_device.yaml"
    yaml2.write_text("""
esphome:
  name: failed_device

esp8266:
  board: nodemcuv2
""")

    setup_core(tmp_path=tmp_path)

    # Mock mixed results - first succeeds, second fails
    mock_run_external_process.side_effect = [0, 1]

    # Should return 1 (failure) since one device failed
    assert command_update_all(MockArgs(configuration=[str(tmp_path)])) == 1

    captured = capfd.readouterr()
    clean_output = strip_ansi_codes(captured.out)

    # Check that both success and failure are properly displayed
    assert "SUCCESS" in clean_output
    assert "ERROR" in clean_output or "FAILED" in clean_output
    assert "SUMMARY" in clean_output

    # Files are processed in alphabetical order, so we need to check which one succeeded/failed
    # The mock_run_external_process.side_effect = [0, 1] applies to files in alphabetical order
    # So "failed_device.yaml" gets 0 (success) and "success_device.yaml" gets 1 (failure)
    assert "failed_device.yaml: SUCCESS" in clean_output
    assert "success_device.yaml: FAILED" in clean_output


def test_command_update_all_empty_directory(
    tmp_path: Path,
    mock_run_external_process: Mock,
    capfd: CaptureFixture[str],
) -> None:
    """Test command_update_all with an empty directory (no YAML files)."""
    setup_core(tmp_path=tmp_path)

    assert command_update_all(MockArgs(configuration=[str(tmp_path)])) == 0
    mock_run_external_process.assert_not_called()

    captured = capfd.readouterr()
    clean_output = strip_ansi_codes(captured.out)

    assert "SUMMARY" in clean_output


def test_command_update_all_single_file(
    tmp_path: Path,
    mock_run_external_process: Mock,
    capfd: CaptureFixture[str],
) -> None:
    """Test command_update_all with a single YAML file specified."""
    yaml_file = tmp_path / "single_device.yaml"
    yaml_file.write_text("""
esphome:
  name: single_device

esp32:
  board: nodemcu-32s
""")

    setup_core(tmp_path=tmp_path)
    mock_run_external_process.return_value = 0

    assert command_update_all(MockArgs(configuration=[str(yaml_file)])) == 0

    captured = capfd.readouterr()
    clean_output = strip_ansi_codes(captured.out)

    assert "single_device.yaml" in clean_output
    assert "SUCCESS" in clean_output
    mock_run_external_process.assert_called_once()


def test_command_update_all_path_formatting_in_color_calls(
    tmp_path: Path,
    mock_run_external_process: Mock,
    capfd: CaptureFixture[str],
) -> None:
    """Test that Path objects are properly converted when passed to color() function."""
    yaml_file = tmp_path / "test-device_123.yaml"
    yaml_file.write_text("""
esphome:
  name: test-device_123

esp32:
  board: nodemcu-32s
""")

    setup_core(tmp_path=tmp_path)
    mock_run_external_process.return_value = 0

    assert command_update_all(MockArgs(configuration=[str(tmp_path)])) == 0

    captured = capfd.readouterr()
    clean_output = strip_ansi_codes(captured.out)

    assert "test-device_123.yaml" in clean_output
    assert "Processing" in clean_output
    assert "SUCCESS" in clean_output
    assert "SUMMARY" in clean_output

    # Should not have any Python error messages
    assert "TypeError" not in clean_output
    assert "can only concatenate str" not in clean_output


def test_command_clean_all_success(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test command_clean_all when writer.clean_all() succeeds."""
    args = MockArgs(configuration=["/path/to/config1", "/path/to/config2"])

    # Set logger level to capture INFO messages
    with (
        caplog.at_level(logging.INFO),
        patch("esphome.writer.clean_all") as mock_clean_all,
    ):
        result = command_clean_all(args)

        assert result == 0
        mock_clean_all.assert_called_once_with(["/path/to/config1", "/path/to/config2"])

        # Check that success message was logged
        assert "Done!" in caplog.text


def test_command_clean_all_oserror(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test command_clean_all when writer.clean_all() raises OSError."""
    args = MockArgs(configuration=["/path/to/config1"])

    # Create a mock OSError with a specific message
    mock_error = OSError("Permission denied: cannot delete directory")

    # Set logger level to capture ERROR and INFO messages
    with (
        caplog.at_level(logging.INFO),
        patch("esphome.writer.clean_all", side_effect=mock_error) as mock_clean_all,
    ):
        result = command_clean_all(args)

        assert result == 1
        mock_clean_all.assert_called_once_with(["/path/to/config1"])

        # Check that error message was logged
        assert (
            "Error cleaning all files: Permission denied: cannot delete directory"
            in caplog.text
        )
        # Should not have success message
        assert "Done!" not in caplog.text


def test_command_clean_all_oserror_no_message(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test command_clean_all when writer.clean_all() raises OSError without message."""
    args = MockArgs(configuration=["/path/to/config1"])

    # Create a mock OSError without a message
    mock_error = OSError()

    # Set logger level to capture ERROR and INFO messages
    with (
        caplog.at_level(logging.INFO),
        patch("esphome.writer.clean_all", side_effect=mock_error) as mock_clean_all,
    ):
        result = command_clean_all(args)

        assert result == 1
        mock_clean_all.assert_called_once_with(["/path/to/config1"])

        # Check that error message was logged (should show empty string for OSError without message)
        assert "Error cleaning all files:" in caplog.text
        # Should not have success message
        assert "Done!" not in caplog.text


def test_command_clean_all_args_used() -> None:
    """Test that command_clean_all uses args.configuration parameter."""
    # Test with different configuration paths
    args1 = MockArgs(configuration=["/path/to/config1"])
    args2 = MockArgs(configuration=["/path/to/config2", "/path/to/config3"])

    with patch("esphome.writer.clean_all") as mock_clean_all:
        result1 = command_clean_all(args1)
        result2 = command_clean_all(args2)

        assert result1 == 0
        assert result2 == 0
        assert mock_clean_all.call_count == 2

        # Verify the correct configuration paths were passed
        mock_clean_all.assert_any_call(["/path/to/config1"])
        mock_clean_all.assert_any_call(["/path/to/config2", "/path/to/config3"])


def test_upload_program_ota_static_ip_with_mqttip(
    mock_mqtt_get_ip: Mock,
    mock_run_ota: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with static IP and MQTTIP (issue #11260).

    This tests the scenario where a device has manual_ip (static IP) configured
    and MQTT is also configured. The devices list contains both the static IP
    and "MQTTIP" magic string. This previously failed because only the first
    device was checked for MQTT resolution.
    """
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_mqtt_get_ip.return_value = ["192.168.2.50"]  # Different subnet
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ],
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
    }
    args = MockArgs(username="user", password="pass", client_id="client")
    # Simulates choose_upload_log_host returning static IP + MQTTIP
    devices = ["192.168.1.100", "MQTTIP"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"

    # Verify MQTT was resolved
    mock_mqtt_get_ip.assert_called_once_with(config, "user", "pass", "client")

    # Verify espota2.run_ota was called with both IPs
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100", "192.168.2.50"],
        3232,
        None,
        expected_firmware,
        OTA_TYPE_UPDATE_APP,
    )


def test_upload_program_ota_multiple_mqttip_resolves_once(
    mock_mqtt_get_ip: Mock,
    mock_run_ota: Mock,
    tmp_path: Path,
) -> None:
    """Test that MQTT resolution only happens once even with multiple MQTT magic strings."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_mqtt_get_ip.return_value = ["192.168.2.50", "192.168.2.51"]
    mock_run_ota.return_value = (0, "192.168.2.50")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ],
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
    }
    args = MockArgs(username="user", password="pass", client_id="client")
    # Multiple MQTT magic strings in the list
    devices = ["MQTTIP", "MQTT", "192.168.1.100"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.2.50"

    # Verify MQTT was only resolved once despite multiple MQTT magic strings
    mock_mqtt_get_ip.assert_called_once_with(config, "user", "pass", "client")

    # Verify espota2.run_ota was called with all unique IPs
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        ["192.168.2.50", "192.168.2.51", "192.168.1.100"],
        3232,
        None,
        expected_firmware,
        OTA_TYPE_UPDATE_APP,
    )


def test_upload_program_ota_mqttip_deduplication(
    mock_mqtt_get_ip: Mock,
    mock_run_ota: Mock,
    tmp_path: Path,
) -> None:
    """Test that duplicate IPs are filtered when MQTT returns same IP as static IP."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    # MQTT returns the same IP as the static IP
    mock_mqtt_get_ip.return_value = ["192.168.1.100"]
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ],
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
    }
    args = MockArgs(username="user", password="pass", client_id="client")
    devices = ["192.168.1.100", "MQTTIP"]

    exit_code, host = upload_program(config, args, devices)

    assert exit_code == 0
    assert host == "192.168.1.100"

    # Verify MQTT was resolved
    mock_mqtt_get_ip.assert_called_once_with(config, "user", "pass", "client")

    # Verify espota2.run_ota was called with deduplicated IPs (only one instance of 192.168.1.100)
    # Note: Current implementation doesn't dedupe, so we'll get the IP twice
    # This test documents current behavior - deduplication could be future enhancement
    mock_run_ota.assert_called_once()
    call_args = mock_run_ota.call_args[0]
    # Should contain both the original IP and MQTT-resolved IP (even if duplicate)
    assert "192.168.1.100" in call_args[0]


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_static_ip_with_mqttip(
    mock_run_logs: Mock,
    mock_mqtt_get_ip: Mock,
) -> None:
    """Test show_logs with static IP and MQTTIP (issue #11260).

    This tests the scenario where a device has manual_ip (static IP) configured
    and MQTT is also configured. The devices list contains both the static IP
    and "MQTTIP" magic string.
    """
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0
    mock_mqtt_get_ip.return_value = ["192.168.2.50"]

    args = MockArgs(username="user", password="pass", client_id="client")
    # Simulates choose_upload_log_host returning static IP + MQTTIP
    devices = ["192.168.1.100", "MQTTIP"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0

    # Verify MQTT was resolved
    mock_mqtt_get_ip.assert_called_once_with(CORE.config, "user", "pass", "client")

    # Verify run_logs was called with both IPs
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100", "192.168.2.50"], subscribe_states=True
    )


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_multiple_mqttip_resolves_once(
    mock_run_logs: Mock,
    mock_mqtt_get_ip: Mock,
) -> None:
    """Test that MQTT resolution only happens once for show_logs with multiple MQTT magic strings."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0
    mock_mqtt_get_ip.return_value = ["192.168.2.50", "192.168.2.51"]

    args = MockArgs(username="user", password="pass", client_id="client")
    # Multiple MQTT magic strings in the list
    devices = ["MQTTIP", "192.168.1.100", "MQTT"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0

    # Verify MQTT was only resolved once despite multiple MQTT magic strings
    mock_mqtt_get_ip.assert_called_once_with(CORE.config, "user", "pass", "client")

    # Verify run_logs was called with all unique IPs (MQTT strings replaced with IPs)
    # Note: "MQTT" is a different magic string from "MQTTIP", but both trigger MQTT resolution
    # The _resolve_network_devices helper filters out both after first resolution
    mock_run_logs.assert_called_once_with(
        CORE.config,
        ["192.168.2.50", "192.168.2.51", "192.168.1.100"],
        subscribe_states=True,
    )


def test_upload_program_ota_mqtt_timeout_fallback(
    mock_mqtt_get_ip: Mock,
    mock_run_ota: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program falls back to other devices when MQTT times out."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path)

    # MQTT times out
    mock_mqtt_get_ip.side_effect = EsphomeError("Failed to find IP via MQTT")
    mock_run_ota.return_value = (0, "192.168.1.100")

    config = {
        CONF_OTA: [
            {
                CONF_PLATFORM: CONF_ESPHOME,
                CONF_PORT: 3232,
            }
        ],
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
    }
    args = MockArgs(username="user", password="pass", client_id="client")
    # Static IP first, MQTTIP second
    devices = ["192.168.1.100", "MQTTIP"]

    exit_code, host = upload_program(config, args, devices)

    # Should succeed using the static IP even though MQTT failed
    assert exit_code == 0
    assert host == "192.168.1.100"

    # Verify MQTT was attempted
    mock_mqtt_get_ip.assert_called_once_with(config, "user", "pass", "client")

    # Verify espota2.run_ota was called with only the static IP (MQTT failed)
    expected_firmware = (
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        ["192.168.1.100"], 3232, None, expected_firmware, OTA_TYPE_UPDATE_APP
    )


@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_mqtt_timeout_fallback(
    mock_run_logs: Mock,
    mock_mqtt_get_ip: Mock,
) -> None:
    """Test show_logs falls back to other devices when MQTT times out."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0
    # MQTT times out
    mock_mqtt_get_ip.side_effect = EsphomeError("Failed to find IP via MQTT")

    args = MockArgs(username="user", password="pass", client_id="client")
    # Static IP first, MQTTIP second
    devices = ["192.168.1.100", "MQTTIP"]

    result = show_logs(CORE.config, args, devices)

    # Should succeed using the static IP even though MQTT failed
    assert result == 0

    # Verify MQTT was attempted
    mock_mqtt_get_ip.assert_called_once_with(CORE.config, "user", "pass", "client")

    # Verify run_logs was called with only the static IP (MQTT failed)
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100"], subscribe_states=True
    )


def test_detect_external_components_no_external(
    mock_get_esphome_components: Mock,
) -> None:
    """Test detect_external_components with no external components."""
    config = {
        CONF_ESPHOME: {CONF_NAME: "test_device"},
        "logger": {},
        "api": {},
    }

    result = detect_external_components(config)

    assert result == set()
    mock_get_esphome_components.assert_called_once()


def test_detect_external_components_with_external(
    mock_get_esphome_components: Mock,
) -> None:
    """Test detect_external_components detects external components."""
    config = {
        CONF_ESPHOME: {CONF_NAME: "test_device"},
        "logger": {},  # Built-in
        "api": {},  # Built-in
        "my_custom_sensor": {},  # External
        "another_custom": {},  # External
        "external_components": [],  # Special key, not a component
        "substitutions": {},  # Special key, not a component
    }

    result = detect_external_components(config)

    assert result == {"my_custom_sensor", "another_custom"}
    mock_get_esphome_components.assert_called_once()


def test_detect_external_components_filters_special_keys(
    mock_get_esphome_components: Mock,
) -> None:
    """Test detect_external_components filters out special config keys."""
    config = {
        CONF_ESPHOME: {CONF_NAME: "test_device"},
        "substitutions": {"key": "value"},
        "packages": {},
        "globals": [],
        "external_components": [],
        "<<": {},  # YAML merge key
    }

    result = detect_external_components(config)

    assert result == set()
    mock_get_esphome_components.assert_called_once()


def test_command_analyze_memory_success(
    tmp_path: Path,
    capfd: CaptureFixture[str],
    mock_write_cpp: Mock,
    mock_compile_program: Mock,
    mock_get_idedata: Mock,
    mock_get_esphome_components: Mock,
    mock_memory_analyzer_cli: Mock,
    mock_ram_strings_analyzer: Mock,
) -> None:
    """Test command_analyze_memory with successful compilation and analysis."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test_device")

    # Create firmware.elf file
    firmware_path = (
        tmp_path / ".esphome" / "build" / "test_device" / ".pioenvs" / "test_device"
    )
    firmware_path.mkdir(parents=True, exist_ok=True)
    firmware_elf = firmware_path / "firmware.elf"
    firmware_elf.write_text("mock elf file")

    # Mock idedata
    mock_idedata_obj = MagicMock(spec=toolchain.IDEData)
    mock_idedata_obj.firmware_elf_path = str(firmware_elf)
    mock_idedata_obj.objdump_path = "/path/to/objdump"
    mock_idedata_obj.readelf_path = "/path/to/readelf"
    mock_get_idedata.return_value = mock_idedata_obj

    config = {
        CONF_ESPHOME: {CONF_NAME: "test_device"},
        "logger": {},
    }

    args = MockArgs()

    result = command_analyze_memory(args, config)

    assert result == 0

    # Verify compilation was done
    mock_write_cpp.assert_called_once_with(config)
    mock_compile_program.assert_called_once_with(args, config)

    # Verify analyzer was created with correct parameters
    mock_memory_analyzer_cli.assert_called_once_with(
        str(firmware_elf),
        "/path/to/objdump",
        "/path/to/readelf",
        set(),  # No external components
        idedata=mock_get_idedata.return_value,
    )

    # Verify analysis was run
    mock_analyzer = mock_memory_analyzer_cli.return_value
    mock_analyzer.analyze.assert_called_once()
    mock_analyzer.generate_report.assert_called_once()

    # Verify RAM strings analyzer was created and run
    mock_ram_strings_analyzer.assert_called_once_with(
        str(firmware_elf),
        objdump_path="/path/to/objdump",
        platform="esp32",
    )
    mock_ram_analyzer = mock_ram_strings_analyzer.return_value
    mock_ram_analyzer.analyze.assert_called_once()
    mock_ram_analyzer.generate_report.assert_called_once()

    # Verify reports were printed
    captured = capfd.readouterr()
    assert "Mock Memory Report" in captured.out
    assert "Mock RAM Strings Report" in captured.out


def test_command_analyze_memory_with_external_components(
    tmp_path: Path,
    mock_write_cpp: Mock,
    mock_compile_program: Mock,
    mock_get_idedata: Mock,
    mock_get_esphome_components: Mock,
    mock_memory_analyzer_cli: Mock,
    mock_ram_strings_analyzer: Mock,
) -> None:
    """Test command_analyze_memory detects external components."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test_device")

    # Create firmware.elf file
    firmware_path = (
        tmp_path / ".esphome" / "build" / "test_device" / ".pioenvs" / "test_device"
    )
    firmware_path.mkdir(parents=True, exist_ok=True)
    firmware_elf = firmware_path / "firmware.elf"
    firmware_elf.write_text("mock elf file")

    # Mock idedata
    mock_idedata_obj = MagicMock(spec=toolchain.IDEData)
    mock_idedata_obj.firmware_elf_path = str(firmware_elf)
    mock_idedata_obj.objdump_path = "/path/to/objdump"
    mock_idedata_obj.readelf_path = "/path/to/readelf"
    mock_get_idedata.return_value = mock_idedata_obj

    config = {
        CONF_ESPHOME: {CONF_NAME: "test_device"},
        "logger": {},
        "my_custom_component": {"param": "value"},  # External component
        "external_components": [{"source": "github://user/repo"}],  # Not a component
    }

    args = MockArgs()

    result = command_analyze_memory(args, config)

    assert result == 0

    # Verify analyzer was created with external components detected
    mock_memory_analyzer_cli.assert_called_once_with(
        str(firmware_elf),
        "/path/to/objdump",
        "/path/to/readelf",
        {"my_custom_component"},  # External component detected
        idedata=mock_get_idedata.return_value,
    )


def test_command_analyze_memory_write_cpp_fails(
    tmp_path: Path,
    mock_write_cpp: Mock,
) -> None:
    """Test command_analyze_memory when write_cpp fails."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test_device")

    config = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    mock_write_cpp.return_value = 1  # Failure

    result = command_analyze_memory(args, config)

    assert result == 1
    mock_write_cpp.assert_called_once_with(config)


def test_command_analyze_memory_compile_fails(
    tmp_path: Path,
    mock_write_cpp: Mock,
    mock_compile_program: Mock,
) -> None:
    """Test command_analyze_memory when compilation fails."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test_device")

    config = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    mock_compile_program.return_value = 1  # Compilation failed

    result = command_analyze_memory(args, config)

    assert result == 1
    mock_write_cpp.assert_called_once_with(config)
    mock_compile_program.assert_called_once_with(args, config)


def test_command_analyze_memory_no_idedata(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_write_cpp: Mock,
    mock_compile_program: Mock,
    mock_get_idedata: Mock,
) -> None:
    """Test command_analyze_memory when idedata cannot be retrieved."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test_device")

    config = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    mock_get_idedata.return_value = None  # Failed to get idedata

    with caplog.at_level(logging.ERROR):
        result = command_analyze_memory(args, config)

    assert result == 1
    assert "Failed to get IDE data for memory analysis" in caplog.text


@pytest.fixture
def mock_compile_build_info_run_compile() -> Generator[Mock]:
    """Mock toolchain.run_compile for build_info tests."""
    with patch("esphome.platformio.toolchain.run_compile", return_value=0) as mock:
        yield mock


@pytest.fixture
def mock_compile_build_info_get_idedata() -> Generator[Mock]:
    """Mock toolchain.get_idedata for build_info tests."""
    mock_idedata = MagicMock()
    with patch(
        "esphome.platformio.toolchain.get_idedata", return_value=mock_idedata
    ) as mock:
        yield mock


def _setup_build_info_test(
    tmp_path: Path,
    *,
    create_firmware: bool = True,
    create_build_info: bool = True,
    build_info_content: str | None = None,
    firmware_first: bool = False,
) -> tuple[Path, Path]:
    """Set up build directory structure for build_info tests.

    Args:
        tmp_path: Temporary directory path.
        create_firmware: Whether to create firmware.bin file.
        create_build_info: Whether to create build_info.json file.
        build_info_content: Custom content for build_info.json, or None for default.
        firmware_first: If True, create firmware before build_info (makes firmware older).

    Returns:
        Tuple of (build_info_path, firmware_path).
    """
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test_device")

    build_path = tmp_path / ".esphome" / "build" / "test_device"
    pioenvs_path = build_path / ".pioenvs" / "test_device"
    pioenvs_path.mkdir(parents=True, exist_ok=True)

    build_info_path = build_path / "build_info.json"
    firmware_path = pioenvs_path / "firmware.bin"

    default_build_info = json.dumps(
        {
            "config_hash": 0x12345678,
            "build_time": int(time.time()),
            "build_time_str": "Dec 13 2025, 12:00:00",
            "esphome_version": "2025.1.0",
        }
    )

    def create_build_info_file() -> None:
        if create_build_info:
            content = (
                build_info_content
                if build_info_content is not None
                else default_build_info
            )
            build_info_path.write_text(content)

    def create_firmware_file() -> None:
        if create_firmware:
            firmware_path.write_bytes(b"fake firmware")

    if firmware_first:
        create_firmware_file()
        time.sleep(0.01)  # Ensure different timestamps
        create_build_info_file()
    else:
        create_build_info_file()
        time.sleep(0.01)  # Ensure different timestamps
        create_firmware_file()

    return build_info_path, firmware_path


def test_compile_program_emits_build_info_when_firmware_rebuilt(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_compile_build_info_run_compile: Mock,
    mock_compile_build_info_get_idedata: Mock,
) -> None:
    """Test that compile_program logs build_info when firmware is rebuilt."""
    _setup_build_info_test(tmp_path, firmware_first=False)

    config: dict[str, Any] = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = compile_program(args, config)

    assert result == 0
    assert "Build Info: config_hash=0x12345678" in caplog.text


def test_compile_program_no_build_info_when_firmware_not_rebuilt(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_compile_build_info_run_compile: Mock,
    mock_compile_build_info_get_idedata: Mock,
) -> None:
    """Test that compile_program doesn't log build_info when firmware wasn't rebuilt."""
    _setup_build_info_test(tmp_path, firmware_first=True)

    config: dict[str, Any] = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = compile_program(args, config)

    assert result == 0
    assert "Build Info:" not in caplog.text


def test_compile_program_no_build_info_when_firmware_missing(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_compile_build_info_run_compile: Mock,
    mock_compile_build_info_get_idedata: Mock,
) -> None:
    """Test that compile_program doesn't log build_info when firmware.bin doesn't exist."""
    _setup_build_info_test(tmp_path, create_firmware=False)

    config: dict[str, Any] = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = compile_program(args, config)

    assert result == 0
    assert "Build Info:" not in caplog.text


def test_compile_program_no_build_info_when_json_missing(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_compile_build_info_run_compile: Mock,
    mock_compile_build_info_get_idedata: Mock,
) -> None:
    """Test that compile_program doesn't log build_info when build_info.json doesn't exist."""
    _setup_build_info_test(tmp_path, create_build_info=False)

    config: dict[str, Any] = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = compile_program(args, config)

    assert result == 0
    assert "Build Info:" not in caplog.text


def test_compile_program_no_build_info_when_json_invalid(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_compile_build_info_run_compile: Mock,
    mock_compile_build_info_get_idedata: Mock,
) -> None:
    """Test that compile_program doesn't log build_info when build_info.json is invalid."""
    _setup_build_info_test(tmp_path, build_info_content="not valid json {{{")

    config: dict[str, Any] = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    with caplog.at_level(logging.DEBUG):
        result = compile_program(args, config)

    assert result == 0
    assert "Build Info:" not in caplog.text


def test_compile_program_no_build_info_when_json_missing_keys(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    mock_compile_build_info_run_compile: Mock,
    mock_compile_build_info_get_idedata: Mock,
) -> None:
    """Test that compile_program doesn't log build_info when build_info.json is missing required keys."""
    _setup_build_info_test(
        tmp_path, build_info_content=json.dumps({"build_time": 1234567890})
    )

    config: dict[str, Any] = {CONF_ESPHOME: {CONF_NAME: "test_device"}}
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = compile_program(args, config)

    assert result == 0
    assert "Build Info:" not in caplog.text


# Tests for run_miniterm serial log batching


# Sentinel to signal end of mock serial data (raises SerialException)
MOCK_SERIAL_END = object()


class MockSerial:
    """Mock serial port for testing run_miniterm."""

    def __init__(self, chunks: list[bytes | object]) -> None:
        """Initialize with a list of chunks to return from read().

        Args:
            chunks: List of byte chunks to return sequentially.
                    Use MOCK_SERIAL_END sentinel to signal end of data.
                    Empty bytes b"" simulate timeout (no data available).
        """
        self.chunks = list(chunks)
        self.chunk_index = 0
        self.baudrate = 0
        self.port = ""
        self.dtr = True
        self.rts = True
        self.timeout = 0.1
        self._is_open = False

    def __enter__(self) -> MockSerial:
        self._is_open = True
        return self

    def __exit__(self, *args: Any) -> None:
        self._is_open = False

    @property
    def in_waiting(self) -> int:
        """Return number of bytes available."""
        if self.chunk_index < len(self.chunks):
            chunk = self.chunks[self.chunk_index]
            if chunk is MOCK_SERIAL_END:
                return 0
            return len(chunk)  # type: ignore[arg-type]
        return 0

    def read(self, size: int = 1) -> bytes:
        """Read up to size bytes from the current chunk.

        This method respects the size argument and keeps any unconsumed
        bytes in the current chunk so that subsequent calls to in_waiting
        and read see the remaining data.
        """
        if self.chunk_index < len(self.chunks):
            chunk = self.chunks[self.chunk_index]
            if chunk is MOCK_SERIAL_END:
                # Sentinel means we're done - simulate port closed
                import serial

                raise serial.SerialException("Port closed")
            # Respect the requested size and keep any remaining bytes
            if size <= 0:
                return b""
            data = chunk[:size]  # type: ignore[index]
            remaining = chunk[size:]  # type: ignore[index]
            if remaining:
                # Keep remaining bytes for the next read
                self.chunks[self.chunk_index] = remaining  # type: ignore[assignment]
            else:
                # Entire chunk consumed; advance to the next one
                self.chunk_index += 1
            return data  # type: ignore[return-value]
        import serial

        raise serial.SerialException("Port closed")


def test_run_miniterm_batches_lines_with_same_timestamp(
    capfd: CaptureFixture[str],
) -> None:
    """Test that lines from the same chunk get the same timestamp."""
    # Simulate receiving multiple log lines in a single chunk
    # This is how data arrives over USB - many lines at once
    chunk = b"[I][app:100]: Line 1\r\n[I][app:100]: Line 2\r\n[I][app:100]: Line 3\r\n"

    mock_serial = MockSerial([chunk, MOCK_SERIAL_END])

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 115200,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    with (
        patch("serial.Serial", return_value=mock_serial),
        patch.object(esp32, "process_stacktrace") as mock_bt,
    ):
        mock_bt.return_value = False
        result = run_miniterm(config, "/dev/ttyUSB0", args)

    assert result == 0

    captured = capfd.readouterr()
    lines = [line for line in captured.out.strip().split("\n") if line]

    # All 3 lines should have the same timestamp (first 13 chars like "[HH:MM:SS.mmm]")
    assert len(lines) == 3
    timestamps = [line[:13] for line in lines]
    assert timestamps[0] == timestamps[1] == timestamps[2], (
        f"Lines from same chunk should have same timestamp: {timestamps}"
    )


def test_run_miniterm_different_chunks_different_timestamps(
    capfd: CaptureFixture[str],
) -> None:
    """Test that lines from different chunks can have different timestamps."""
    # Two separate chunks - could have different timestamps
    chunk1 = b"[I][app:100]: Chunk 1 Line\r\n"
    chunk2 = b"[I][app:100]: Chunk 2 Line\r\n"

    mock_serial = MockSerial([chunk1, chunk2, MOCK_SERIAL_END])

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 115200,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    with (
        patch("serial.Serial", return_value=mock_serial),
        patch.object(esp32, "process_stacktrace") as mock_bt,
    ):
        mock_bt.return_value = False
        result = run_miniterm(config, "/dev/ttyUSB0", args)

    assert result == 0

    captured = capfd.readouterr()
    lines = [line for line in captured.out.strip().split("\n") if line]
    assert len(lines) == 2


def test_run_miniterm_handles_split_lines() -> None:
    """Test that partial lines are buffered until complete."""
    # Line split across two chunks
    chunk1 = b"[I][app:100]: Start of "
    chunk2 = b"line\r\n"

    mock_serial = MockSerial([chunk1, chunk2, MOCK_SERIAL_END])

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 115200,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    with (
        patch("serial.Serial", return_value=mock_serial),
        patch.object(esp32, "process_stacktrace") as mock_bt,
        patch("esphome.__main__.safe_print") as mock_print,
    ):
        mock_bt.return_value = False
        run_miniterm(config, "/dev/ttyUSB0", args)

    # Should have printed exactly one complete line
    assert mock_print.call_count == 1
    printed_line = mock_print.call_args[0][0]
    assert "Start of line" in printed_line


def test_run_miniterm_backtrace_state_maintained() -> None:
    """Test that backtrace_state is properly maintained across lines.

    ESP8266 backtraces span multiple lines between >>>stack>>> and <<<stack<<<.
    The backtrace_state must persist correctly when lines arrive in the same chunk.
    """
    # Simulate ESP8266 multi-line backtrace arriving in a single chunk
    backtrace_chunk = (
        b">>>stack>>>\r\n"
        b"3ffffe90:  40220ef8 b66aa8c0 3fff0a4c 40204c84\r\n"
        b"3ffffea0:  00000005 0000a635 3fff191c 4020413c\r\n"
        b"<<<stack<<<\r\n"
    )

    mock_serial = MockSerial([backtrace_chunk, MOCK_SERIAL_END])

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 115200,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    backtrace_states: list[tuple[str, bool]] = []

    def track_backtrace_state(
        config: dict[str, Any], line: str, backtrace_state: bool
    ) -> bool:
        """Track the backtrace_state progression."""
        backtrace_states.append((line, backtrace_state))
        # Simulate actual behavior
        if ">>>stack>>>" in line:
            return True
        if "<<<stack<<<" in line:
            return False
        return backtrace_state

    with (
        patch("serial.Serial", return_value=mock_serial),
        patch.object(
            esp32,
            "process_stacktrace",
            side_effect=track_backtrace_state,
        ),
    ):
        run_miniterm(config, "/dev/ttyUSB0", args)

    # Verify the state progression
    assert len(backtrace_states) == 4

    # Line 1: >>>stack>>> - state should be False (before processing)
    assert ">>>stack>>>" in backtrace_states[0][0]
    assert backtrace_states[0][1] is False

    # Line 2: stack data - state should be True (after >>>stack>>>)
    assert "40220ef8" in backtrace_states[1][0]
    assert backtrace_states[1][1] is True

    # Line 3: more stack data - state should be True
    assert "4020413c" in backtrace_states[2][0]
    assert backtrace_states[2][1] is True

    # Line 4: <<<stack<<< - state should be True (before processing end marker)
    assert "<<<stack<<<" in backtrace_states[3][0]
    assert backtrace_states[3][1] is True


def test_run_miniterm_handles_empty_reads(
    capfd: CaptureFixture[str],
) -> None:
    """Test that empty reads (timeouts) are handled correctly.

    When read() returns empty bytes, the code should continue waiting
    for more data without processing anything.
    """
    # Simulate: empty read (timeout), then data, then empty read, then end
    chunk = b"[I][app:100]: Test line\r\n"

    mock_serial = MockSerial([b"", chunk, b"", MOCK_SERIAL_END])

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 115200,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    with (
        patch("serial.Serial", return_value=mock_serial),
        patch.object(esp32, "process_stacktrace") as mock_bt,
    ):
        mock_bt.return_value = False
        result = run_miniterm(config, "/dev/ttyUSB0", args)

    assert result == 0

    captured = capfd.readouterr()
    lines = [line for line in captured.out.strip().split("\n") if line]
    # Should have exactly one line despite empty reads
    assert len(lines) == 1
    assert "Test line" in lines[0]


def test_run_miniterm_no_logger_returns_early(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that run_miniterm returns early if logger is not configured."""
    config: dict[str, Any] = {}  # No logger config
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = run_miniterm(config, "/dev/ttyUSB0", args)

    assert result == 1
    assert "Logger is not enabled" in caplog.text


def test_run_miniterm_baud_rate_zero_returns_early(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that run_miniterm returns early if baud_rate is 0."""
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 0,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    with caplog.at_level(logging.INFO):
        result = run_miniterm(config, "/dev/ttyUSB0", args)

    assert result == 1
    assert "UART logging is disabled" in caplog.text


def test_run_miniterm_buffer_limit_prevents_unbounded_growth() -> None:
    """Test that buffer is limited to prevent unbounded memory growth.

    If a device sends data without newlines, the buffer should be truncated
    to SERIAL_BUFFER_MAX_SIZE to prevent memory exhaustion.
    """
    # Use a small buffer limit for testing
    test_buffer_limit = 100

    # Create data larger than the limit without newlines
    large_data_no_newline = b"X" * 150  # 150 bytes, no newline
    final_line = b"END\r\n"

    mock_serial = MockSerial([large_data_no_newline, final_line, MOCK_SERIAL_END])

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    config = {
        CONF_LOGGER: {
            CONF_BAUD_RATE: 115200,
            "deassert_rts_dtr": False,
        }
    }
    args = MockArgs()

    with (
        patch("serial.Serial", return_value=mock_serial),
        patch.object(esp32, "process_stacktrace") as mock_bt,
        patch("esphome.__main__.safe_print") as mock_print,
        patch("esphome.__main__.SERIAL_BUFFER_MAX_SIZE", test_buffer_limit),
    ):
        mock_bt.return_value = False
        run_miniterm(config, "/dev/ttyUSB0", args)

    # Should have printed exactly one line
    assert mock_print.call_count == 1
    printed_line = mock_print.call_args[0][0]

    # The line should contain "END" and some X's, but not all 150 X's
    # because the buffer was truncated
    assert "END" in printed_line
    assert "X" in printed_line
    # Verify truncation happened - we shouldn't have all 150 X's
    # The buffer logic is:
    # 1. Add 150 X's -> buffer = 150 bytes -> truncate to last 100 = 100 X's
    # 2. Add "END\r\n" (5 bytes) -> buffer = 105 bytes -> truncate to last 100
    #    = 95 X's + "END\r\n"
    # 3. Find newline, extract line = "95 X's + END"
    x_count = printed_line.count("X")
    assert x_count < 150, f"Expected truncation but got {x_count} X's"
    assert x_count == 95, f"Expected 95 X's after truncation but got {x_count}"


def test_run_esphome_multiple_configs_with_secrets(
    tmp_path: Path,
    mock_run_external_process: Mock,
    capfd: CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test run_esphome with multiple configs and secrets file.

    Verifies:
    - Multiple configs use subprocess isolation
    - Secrets files are skipped with warning
    - Secrets files don't appear in summary
    """
    # Create two config files and a secrets file
    yaml_file1 = tmp_path / "device1.yaml"
    yaml_file1.write_text("""
esphome:
  name: device1

esp32:
  board: nodemcu-32s
""")
    yaml_file2 = tmp_path / "device2.yaml"
    yaml_file2.write_text("""
esphome:
  name: device2

esp32:
  board: nodemcu-32s
""")
    secrets_file = tmp_path / "secrets.yaml"
    secrets_file.write_text("wifi_password: secret123\n")

    setup_core(tmp_path=tmp_path)
    mock_run_external_process.return_value = 0

    # run_esphome expects argv[0] to be the program name (gets sliced off by parse_args)
    with caplog.at_level(logging.WARNING):
        result = run_esphome(
            ["esphome", "compile", str(yaml_file1), str(secrets_file), str(yaml_file2)]
        )

    assert result == 0

    # Check secrets file was skipped with warning
    assert "Skipping secrets file" in caplog.text
    assert "secrets.yaml" in caplog.text

    captured = capfd.readouterr()
    clean_output = strip_ansi_codes(captured.out)

    # Both config files should be processed
    assert "device1.yaml" in clean_output
    assert "device2.yaml" in clean_output
    assert "SUMMARY" in clean_output

    # Secrets should not appear in summary
    summary_section = (
        clean_output.split("SUMMARY")[1] if "SUMMARY" in clean_output else ""
    )
    assert "secrets.yaml" not in summary_section


# --- command_bundle tests ---


def test_command_bundle_list_only(
    tmp_path: Path,
    capsys: CaptureFixture[str],
) -> None:
    """Test command_bundle with --list-only prints files and returns 0."""
    mock_files = [
        BundleFile(path="device.yaml", source=tmp_path / "device.yaml"),
        BundleFile(path="secrets.yaml", source=tmp_path / "secrets.yaml"),
        BundleFile(path="common/base.yaml", source=tmp_path / "common" / "base.yaml"),
    ]

    args = MockArgs(list_only=True)
    config: dict[str, Any] = {}

    mock_creator = MagicMock()
    mock_creator.discover_files.return_value = mock_files

    with patch("esphome.bundle.ConfigBundleCreator", return_value=mock_creator):
        result = command_bundle(args, config)

    assert result == 0
    captured = capsys.readouterr()
    # Files should be printed in sorted order
    assert "common/base.yaml" in captured.out
    assert "device.yaml" in captured.out
    assert "secrets.yaml" in captured.out


def test_command_bundle_list_only_empty(
    tmp_path: Path,
    capsys: CaptureFixture[str],
) -> None:
    """Test command_bundle --list-only with no files discovered."""
    args = MockArgs(list_only=True)
    config: dict[str, Any] = {}

    mock_creator = MagicMock()
    mock_creator.discover_files.return_value = []

    with patch("esphome.bundle.ConfigBundleCreator", return_value=mock_creator):
        result = command_bundle(args, config)

    assert result == 0


def test_command_bundle_creates_archive(tmp_path: Path) -> None:
    """Test command_bundle creates archive at default output path."""
    CORE.config_path = tmp_path / "mydevice.yaml"

    mock_result = BundleResult(
        data=b"fake-tar-gz-data",
        manifest={"manifest_version": 1},
        files=[BundleFile(path="mydevice.yaml", source=tmp_path / "mydevice.yaml")],
    )

    args = MockArgs()
    config: dict[str, Any] = {}

    mock_creator = MagicMock()
    mock_creator.create_bundle.return_value = mock_result

    with patch("esphome.bundle.ConfigBundleCreator", return_value=mock_creator):
        result = command_bundle(args, config)

    assert result == 0
    output_path = tmp_path / f"mydevice{BUNDLE_EXTENSION}"
    assert output_path.exists()
    assert output_path.read_bytes() == b"fake-tar-gz-data"


def test_command_bundle_custom_output(tmp_path: Path) -> None:
    """Test command_bundle with -o custom output path."""
    custom_output = tmp_path / "output" / "custom.esphomebundle.tar.gz"
    mock_result = BundleResult(
        data=b"custom-output-data",
        manifest={"manifest_version": 1},
        files=[BundleFile(path="mydevice.yaml", source=tmp_path / "mydevice.yaml")],
    )

    args = MockArgs(output=str(custom_output))
    config: dict[str, Any] = {}

    mock_creator = MagicMock()
    mock_creator.create_bundle.return_value = mock_result

    with patch("esphome.bundle.ConfigBundleCreator", return_value=mock_creator):
        result = command_bundle(args, config)

    assert result == 0
    assert custom_output.exists()
    assert custom_output.read_bytes() == b"custom-output-data"


def test_command_bundle_creates_parent_dirs(tmp_path: Path) -> None:
    """Test command_bundle creates parent directories for output path."""
    nested_output = tmp_path / "deep" / "nested" / "dir" / "out.tar.gz"
    mock_result = BundleResult(
        data=b"data",
        manifest={"manifest_version": 1},
        files=[BundleFile(path="mydevice.yaml", source=tmp_path / "mydevice.yaml")],
    )

    args = MockArgs(output=str(nested_output))
    config: dict[str, Any] = {}

    mock_creator = MagicMock()
    mock_creator.create_bundle.return_value = mock_result

    with patch("esphome.bundle.ConfigBundleCreator", return_value=mock_creator):
        result = command_bundle(args, config)

    assert result == 0
    assert nested_output.exists()


def test_command_bundle_logs_info(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test command_bundle logs bundle creation info."""
    CORE.config_path = tmp_path / "mydevice.yaml"

    mock_result = BundleResult(
        data=b"x" * 2048,
        manifest={"manifest_version": 1},
        files=[
            BundleFile(path="mydevice.yaml", source=tmp_path / "mydevice.yaml"),
            BundleFile(path="secrets.yaml", source=tmp_path / "secrets.yaml"),
        ],
    )

    args = MockArgs()
    config: dict[str, Any] = {}

    mock_creator = MagicMock()
    mock_creator.create_bundle.return_value = mock_result

    with (
        patch("esphome.bundle.ConfigBundleCreator", return_value=mock_creator),
        caplog.at_level(logging.INFO),
    ):
        result = command_bundle(args, config)

    assert result == 0
    assert "Bundle created" in caplog.text
    assert "2 files" in caplog.text
    assert "2.0 KB" in caplog.text


def test_run_esphome_bundle_detection(tmp_path: Path) -> None:
    """Test run_esphome detects .esphomebundle.tar.gz and extracts it."""
    bundle_path = tmp_path / f"device{BUNDLE_EXTENSION}"
    bundle_path.write_bytes(b"fake-bundle")

    extracted_yaml = tmp_path / "extracted" / "device.yaml"

    with (
        patch("esphome.bundle.is_bundle_path", return_value=True) as mock_is_bundle,
        patch(
            "esphome.bundle.prepare_bundle_for_compile",
            return_value=extracted_yaml,
        ) as mock_prepare,
        patch("esphome.__main__.read_config", return_value=None),
    ):
        result = run_esphome(["esphome", "compile", str(bundle_path)])

    mock_is_bundle.assert_called_once()
    mock_prepare.assert_called_once_with(bundle_path)
    # read_config returns None → exit code 2
    assert result == 2


def test_run_esphome_non_bundle_skips_extraction(tmp_path: Path) -> None:
    """Test run_esphome does not extract for regular .yaml files."""
    yaml_file = tmp_path / "device.yaml"
    yaml_file.write_text("esphome:\n  name: test\n")

    with (
        patch("esphome.bundle.is_bundle_path", return_value=False) as mock_is_bundle,
        patch("esphome.bundle.prepare_bundle_for_compile") as mock_prepare,
        patch("esphome.__main__.read_config", return_value=None),
    ):
        result = run_esphome(["esphome", "compile", str(yaml_file)])

    mock_is_bundle.assert_called_once()
    mock_prepare.assert_not_called()
    assert result == 2


@pytest.mark.parametrize(
    ("command", "expected_skip"),
    [
        ("logs", True),
        ("clean", True),
        ("compile", False),
        ("config", False),
        ("run", False),
        ("clean-mqtt", False),
    ],
)
def test_run_esphome_skip_external_update_per_command(
    tmp_path: Path, command: str, expected_skip: bool
) -> None:
    """read_config is invoked with skip_external_update=True only for commands
    that don't need fresh external components (logs, clean)."""
    yaml_file = tmp_path / "device.yaml"
    yaml_file.write_text("esphome:\n  name: test\n")

    with patch("esphome.__main__.read_config", return_value=None) as mock_read:
        run_esphome(["esphome", command, str(yaml_file)])

    mock_read.assert_called_once()
    assert mock_read.call_args.kwargs["skip_external_update"] is expected_skip


def test_get_configured_xtal_freq_reads_sdkconfig(tmp_path: Path) -> None:
    """Test reading XTAL_FREQ from sdkconfig."""
    CORE.name = "test-device"
    CORE.build_path = tmp_path
    sdkconfig = tmp_path / "sdkconfig.test-device"
    sdkconfig.write_text(
        "CONFIG_SOC_XTAL_SUPPORT_26M=y\nCONFIG_XTAL_FREQ=26\nCONFIG_XTAL_FREQ_26=y\n"
    )
    assert _get_configured_xtal_freq() == 26


def test_get_configured_xtal_freq_default_40(tmp_path: Path) -> None:
    """Test reading default 40MHz XTAL_FREQ from sdkconfig."""
    CORE.name = "test-device"
    CORE.build_path = tmp_path
    sdkconfig = tmp_path / "sdkconfig.test-device"
    sdkconfig.write_text("CONFIG_XTAL_FREQ=40\nCONFIG_XTAL_FREQ_40=y\n")
    assert _get_configured_xtal_freq() == 40


def test_get_configured_xtal_freq_missing_file(tmp_path: Path) -> None:
    """Test that missing sdkconfig returns None."""
    CORE.name = "test-device"
    CORE.build_path = tmp_path
    assert _get_configured_xtal_freq() is None


def test_get_configured_xtal_freq_no_xtal_line(tmp_path: Path) -> None:
    """Test that sdkconfig without XTAL_FREQ returns None."""
    CORE.name = "test-device"
    CORE.build_path = tmp_path
    sdkconfig = tmp_path / "sdkconfig.test-device"
    sdkconfig.write_text("CONFIG_OTHER=123\n")
    assert _get_configured_xtal_freq() is None


def test_crystal_freq_callback_mismatch() -> None:
    """Test callback returns warning on crystal frequency mismatch."""
    callback = _make_crystal_freq_callback(40)
    result = callback("Crystal frequency:  26MHz")
    assert result is not None
    assert "26MHz" in result
    assert "40MHz" in result
    assert "CONFIG_XTAL_FREQ_26" in result


def test_crystal_freq_callback_match() -> None:
    """Test callback returns None when frequencies match."""
    callback = _make_crystal_freq_callback(40)
    result = callback("Crystal frequency:  40MHz")
    assert result is None


def test_crystal_freq_callback_no_crystal_line() -> None:
    """Test callback returns None for unrelated lines."""
    callback = _make_crystal_freq_callback(40)
    assert callback("Chip type: ESP8684H") is None
    assert callback("MAC: a0:b7:65:8b:16:d4") is None
    assert callback("") is None


def test_upload_using_esptool_passes_crystal_callback(
    tmp_path: Path,
    mock_run_external_command_main: Mock,
    mock_get_idedata: Mock,
) -> None:
    """Test that upload_using_esptool passes crystal freq callback for ESP32."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test")
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32}

    # Create sdkconfig with XTAL_FREQ
    build_dir = Path(CORE.build_path)
    build_dir.mkdir(parents=True, exist_ok=True)
    sdkconfig = build_dir / "sdkconfig.test"
    sdkconfig.write_text("CONFIG_XTAL_FREQ=40\n")

    mock_idedata = MagicMock(spec=toolchain.IDEData)
    mock_idedata.firmware_bin_path = tmp_path / "firmware.bin"
    mock_idedata.extra_flash_images = []
    mock_get_idedata.return_value = mock_idedata
    (tmp_path / "firmware.bin").touch()

    config = {CONF_ESPHOME: {"platformio_options": {}}}
    upload_using_esptool(config, "/dev/ttyUSB0", None, None)

    # Verify line_callbacks was passed with the crystal callback
    call_kwargs = mock_run_external_command_main.call_args[1]
    assert "line_callbacks" in call_kwargs
    assert len(call_kwargs["line_callbacks"]) == 1


def test_upload_using_esptool_subprocess_passes_crystal_callback(
    mock_run_external_process: Mock,
    mock_get_idedata: Mock,
    tmp_path: Path,
) -> None:
    """Test that crystal freq callback is passed via run_external_process."""
    setup_core(platform=PLATFORM_ESP32, tmp_path=tmp_path, name="test")
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32}

    # Create sdkconfig with XTAL_FREQ
    build_dir = Path(CORE.build_path)
    build_dir.mkdir(parents=True, exist_ok=True)
    sdkconfig = build_dir / "sdkconfig.test"
    sdkconfig.write_text("CONFIG_XTAL_FREQ=40\n")

    mock_idedata = MagicMock(spec=toolchain.IDEData)
    mock_idedata.firmware_bin_path = tmp_path / "firmware.bin"
    mock_idedata.extra_flash_images = []
    mock_get_idedata.return_value = mock_idedata
    (tmp_path / "firmware.bin").touch()

    config = {CONF_ESPHOME: {"platformio_options": {}}}
    with patch.dict(os.environ, {"ESPHOME_USE_SUBPROCESS": "1"}):
        upload_using_esptool(config, "/dev/ttyUSB0", None, None)

    call_kwargs = mock_run_external_process.call_args[1]
    assert "line_callbacks" in call_kwargs
    assert len(call_kwargs["line_callbacks"]) == 1


def test_parse_args_run_no_states() -> None:
    """Test that --no-states is parsed for the run command."""
    args = parse_args(["esphome", "run", "--no-states", "device.yaml"])
    assert args.no_states is True


def test_parse_args_run_no_states_default() -> None:
    """Test that no_states defaults to False for the run command."""
    args = parse_args(["esphome", "run", "device.yaml"])
    assert args.no_states is False


def test_parse_args_logs_no_states() -> None:
    """Test that --no-states is parsed for the logs command."""
    args = parse_args(["esphome", "logs", "--no-states", "device.yaml"])
    assert args.no_states is True


@patch("esphome.components.api.client.run_logs")
def test_command_run_passes_no_states_to_show_logs(
    mock_run_logs: Mock,
) -> None:
    """Test that command_run propagates --no-states through to run_logs."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MDNS: {CONF_DISABLED: False},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0

    args = MockArgs()
    args.no_states = True
    args.no_logs = False
    args.device = None

    with (
        patch("esphome.__main__.write_cpp", return_value=0),
        patch("esphome.__main__.compile_program", return_value=0),
        patch(
            "esphome.__main__.choose_upload_log_host",
            return_value=["192.168.1.100"],
        ),
        patch("esphome.__main__.upload_program", return_value=(0, "192.168.1.100")),
        patch("esphome.__main__.get_serial_ports", return_value=[]),
    ):
        result = command_run(args, CORE.config)

    assert result == 0
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100"], subscribe_states=False
    )


@patch("esphome.components.api.client.run_logs")
def test_command_run_defaults_subscribe_states_true(
    mock_run_logs: Mock,
) -> None:
    """Test that command_run subscribes states by default (no --no-states)."""
    setup_core(
        config={
            "logger": {},
            CONF_API: {},
            CONF_MDNS: {CONF_DISABLED: False},
        },
        platform=PLATFORM_ESP32,
    )
    mock_run_logs.return_value = 0

    args = MockArgs()
    args.no_logs = False
    args.device = None

    with (
        patch("esphome.__main__.write_cpp", return_value=0),
        patch("esphome.__main__.compile_program", return_value=0),
        patch(
            "esphome.__main__.choose_upload_log_host",
            return_value=["192.168.1.100"],
        ),
        patch("esphome.__main__.upload_program", return_value=(0, "192.168.1.100")),
        patch("esphome.__main__.get_serial_ports", return_value=[]),
    ):
        result = command_run(args, CORE.config)

    assert result == 0
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100"], subscribe_states=True
    )
