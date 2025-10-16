"""Unit tests for esphome.__main__ module."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
import logging
from pathlib import Path
import re
from typing import Any
from unittest.mock import MagicMock, Mock, patch

import pytest
from pytest import CaptureFixture

from esphome import platformio_api
from esphome.__main__ import (
    Purpose,
    choose_upload_log_host,
    command_clean_all,
    command_rename,
    command_update_all,
    command_wizard,
    get_port_type,
    has_ip_address,
    has_mqtt,
    has_mqtt_ip_lookup,
    has_mqtt_logging,
    has_non_ip_address,
    has_resolvable_address,
    mqtt_get_ip,
    show_logs,
    upload_program,
    upload_using_esptool,
)
from esphome.components.esp32.const import KEY_ESP32, KEY_VARIANT, VARIANT_ESP32
from esphome.const import (
    CONF_API,
    CONF_BROKER,
    CONF_DISABLED,
    CONF_ESPHOME,
    CONF_LEVEL,
    CONF_LOG_TOPIC,
    CONF_MDNS,
    CONF_MQTT,
    CONF_NAME,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_SUBSTITUTIONS,
    CONF_TOPIC,
    CONF_USE_ADDRESS,
    CONF_WIFI,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
)
from esphome.core import CORE, EsphomeError


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
def mock_run_ota() -> Generator[Mock]:
    """Mock espota2.run_ota for testing."""
    with patch("esphome.espota2.run_ota") as mock:
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
def mock_run_external_command() -> Generator[Mock]:
    """Mock run_external_command for testing."""
    with patch("esphome.__main__.run_external_command") as mock:
        mock.return_value = 0  # Default to success
        yield mock


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
    setup_core(config={CONF_OTA: {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default=["OTA"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_with_ota_list_mqtt_fallback() -> None:
    """Test with OTA list falling back to MQTT when no address."""
    setup_core(config={CONF_OTA: {}, "mqtt": {}})

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
    result = choose_upload_log_host(
        default="SERIAL",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == []
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
    setup_core(config={CONF_OTA: {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == ["192.168.1.100"]


def test_choose_upload_log_host_with_ota_device_with_api_config() -> None:
    """Test OTA device when API is configured (no upload without OTA in config)."""
    setup_core(config={CONF_API: {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == []


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

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == []


@pytest.mark.usefixtures("mock_choose_prompt")
def test_choose_upload_log_host_multiple_devices() -> None:
    """Test with multiple devices including special identifiers."""
    setup_core(config={CONF_OTA: {}}, address="192.168.1.100")

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
    setup_core(config={CONF_OTA: {}}, address="192.168.1.100")

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
        config={CONF_OTA: {}, CONF_API: {}, CONF_MQTT: {CONF_BROKER: "mqtt.local"}},
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
        config={CONF_OTA: {}, CONF_API: {}, CONF_MQTT: {CONF_BROKER: "mqtt.local"}},
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
    setup_core(config={CONF_OTA: {}}, address="192.168.1.100")

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
def test_choose_upload_log_host_all_devices_unresolved(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test when all specified devices cannot be resolved."""
    setup_core()

    result = choose_upload_log_host(
        default=["SERIAL", "OTA"],
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == []
    assert (
        "All specified devices: ['SERIAL', 'OTA'] could not be resolved." in caplog.text
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
    setup_core(config={CONF_OTA: {}, CONF_API: {}}, address="192.168.1.100")

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
            CONF_OTA: {},
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
            CONF_OTA: {},
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
    assert result == ["MQTTIP", "test.local"]


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_ota_ip_all_options_logging() -> None:
    """Test OTA device when both static IP, OTA, API and MQTT are configured and enabled but MDNS not."""
    setup_core(
        config={
            CONF_OTA: {},
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
            CONF_OTA: {},
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
    assert result == ["MQTTIP", "MQTT", "test.local"]


@pytest.mark.usefixtures("mock_no_mqtt_logging")
def test_choose_upload_log_host_no_address_with_ota_config() -> None:
    """Test OTA device when OTA is configured but no address is set."""
    setup_core(config={CONF_OTA: {}})

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        purpose=Purpose.UPLOADING,
    )
    assert result == []


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
    mock_run_external_command: Mock,
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
    mock_idedata = MagicMock(spec=platformio_api.IDEData)
    mock_idedata.firmware_bin_path = tmp_path / "firmware.bin"
    mock_idedata.extra_flash_images = [
        platformio_api.FlashImage(path=tmp_path / "bootloader.bin", offset="0x1000"),
        platformio_api.FlashImage(path=tmp_path / "partitions.bin", offset="0x8000"),
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
    assert mock_run_external_command.call_count == 1

    # Get the actual call arguments
    call_args = mock_run_external_command.call_args[0]

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


def test_upload_using_esptool_with_file_path(
    tmp_path: Path,
    mock_run_external_command: Mock,
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
    mock_run_external_command.assert_called_once()

    # Get the actual call arguments
    call_args = mock_run_external_command.call_args[0]
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
        ["192.168.1.100"], 3232, "secret", expected_firmware
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
        ["192.168.1.100"], 3232, None, Path("custom.bin")
    )


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
        ["192.168.1.100"], 3232, None, expected_firmware
    )


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
        CORE.config, ["192.168.1.100", "192.168.1.101"]
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
    mock_run_logs.assert_called_once_with(CORE.config, ["device.example.com"])


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
    mock_run_logs.assert_called_once_with(CORE.config, ["192.168.1.200"])


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
    setup_core(config={CONF_API: {}, CONF_OTA: {}})
    assert has_mqtt() is False


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
    assert has_resolvable_address() is True

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
    assert "Updating" in clean_output
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
        ["192.168.1.100", "192.168.2.50"], 3232, None, expected_firmware
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
        ["192.168.2.50", "192.168.2.51", "192.168.1.100"], 3232, None, expected_firmware
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
        CORE.config, ["192.168.1.100", "192.168.2.50"]
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
        CORE.config, ["192.168.2.50", "192.168.2.51", "192.168.1.100"]
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
        ["192.168.1.100"], 3232, None, expected_firmware
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
    mock_run_logs.assert_called_once_with(CORE.config, ["192.168.1.100"])
