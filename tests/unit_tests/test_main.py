"""Unit tests for esphome.__main__ module."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
from typing import Any
from unittest.mock import Mock, patch

import pytest

from esphome.__main__ import choose_upload_log_host
from esphome.const import CONF_BROKER, CONF_MQTT, CONF_USE_ADDRESS, CONF_WIFI
from esphome.core import CORE


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
    config: dict[str, Any] | None = None, address: str | None = None
) -> None:
    """
    Helper to set up CORE configuration with optional address.

    Args:
        config (dict[str, Any] | None): The configuration dictionary to set for CORE. If None, an empty dict is used.
        address (str | None): Optional network address to set in the configuration. If provided, it is set under the wifi config.
    """
    if config is None:
        config = {}

    if address is not None:
        # Set address via wifi config (could also use ethernet)
        config[CONF_WIFI] = {CONF_USE_ADDRESS: address}

    CORE.config = config


@pytest.fixture
def mock_no_serial_ports() -> Generator[Mock]:
    """Mock get_serial_ports to return no ports."""
    with patch("esphome.__main__.get_serial_ports", return_value=[]) as mock:
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


def test_choose_upload_log_host_with_string_default() -> None:
    """Test with a single string default device."""
    result = choose_upload_log_host(
        default="192.168.1.100",
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["192.168.1.100"]


def test_choose_upload_log_host_with_list_default() -> None:
    """Test with a list of default devices."""
    result = choose_upload_log_host(
        default=["192.168.1.100", "192.168.1.101"],
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["192.168.1.100", "192.168.1.101"]


def test_choose_upload_log_host_with_multiple_ip_addresses() -> None:
    """Test with multiple IP addresses as defaults."""
    result = choose_upload_log_host(
        default=["1.2.3.4", "4.5.5.6"],
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["1.2.3.4", "4.5.5.6"]


def test_choose_upload_log_host_with_mixed_hostnames_and_ips() -> None:
    """Test with a mix of hostnames and IP addresses."""
    result = choose_upload_log_host(
        default=["host.one", "host.one.local", "1.2.3.4"],
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["host.one", "host.one.local", "1.2.3.4"]


def test_choose_upload_log_host_with_ota_list() -> None:
    """Test with OTA as the only item in the list."""
    setup_core(config={"ota": {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default=["OTA"],
        check_default=None,
        show_ota=True,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_with_ota_list_mqtt_fallback() -> None:
    """Test with OTA list falling back to MQTT when no address."""
    setup_core()

    result = choose_upload_log_host(
        default=["OTA"],
        check_default=None,
        show_ota=False,
        show_mqtt=True,
        show_api=False,
    )
    assert result == ["MQTT"]


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_with_serial_device_no_ports(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test SERIAL device when no serial ports are found."""
    result = choose_upload_log_host(
        default="SERIAL",
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=False,
    )
    assert result == []
    assert "No serial ports found, skipping SERIAL device" in caplog.text


@pytest.mark.usefixtures("mock_serial_ports")
def test_choose_upload_log_host_with_serial_device_with_ports(
    mock_choose_prompt: Mock,
) -> None:
    """Test SERIAL device when serial ports are available."""
    result = choose_upload_log_host(
        default="SERIAL",
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=False,
        purpose="testing",
    )
    assert result == ["/dev/ttyUSB0"]
    mock_choose_prompt.assert_called_once_with(
        [
            ("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0"),
            ("/dev/ttyUSB1 (Another USB Serial)", "/dev/ttyUSB1"),
        ],
        purpose="testing",
    )


def test_choose_upload_log_host_with_ota_device_with_ota_config() -> None:
    """Test OTA device when OTA is configured."""
    setup_core(config={"ota": {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        show_ota=True,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["192.168.1.100"]


def test_choose_upload_log_host_with_ota_device_with_api_config() -> None:
    """Test OTA device when API is configured."""
    setup_core(config={"api": {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        show_ota=False,
        show_mqtt=False,
        show_api=True,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_with_ota_device_fallback_to_mqtt() -> None:
    """Test OTA device fallback to MQTT when no OTA/API config."""
    setup_core()

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        show_ota=False,
        show_mqtt=True,
        show_api=False,
    )
    assert result == ["MQTT"]


@pytest.mark.usefixtures("mock_no_mqtt_logging")
def test_choose_upload_log_host_with_ota_device_no_fallback() -> None:
    """Test OTA device with no valid fallback options."""
    setup_core()

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        show_ota=True,
        show_mqtt=True,
        show_api=False,
    )
    assert result == []


@pytest.mark.usefixtures("mock_choose_prompt")
def test_choose_upload_log_host_multiple_devices() -> None:
    """Test with multiple devices including special identifiers."""
    setup_core(config={"ota": {}}, address="192.168.1.100")

    mock_ports = [MockSerialPort("/dev/ttyUSB0", "USB Serial")]

    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports):
        result = choose_upload_log_host(
            default=["192.168.1.50", "OTA", "SERIAL"],
            check_default=None,
            show_ota=True,
            show_mqtt=False,
            show_api=False,
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
            show_ota=False,
            show_mqtt=False,
            show_api=False,
            purpose="uploading",
        )
        assert result == ["/dev/ttyUSB0"]
        mock_choose_prompt.assert_called_once_with(
            [("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0")],
            purpose="uploading",
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_no_defaults_with_ota() -> None:
    """Test interactive mode with OTA option."""
    setup_core(config={"ota": {}}, address="192.168.1.100")

    with patch(
        "esphome.__main__.choose_prompt", return_value="192.168.1.100"
    ) as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            show_ota=True,
            show_mqtt=False,
            show_api=False,
        )
        assert result == ["192.168.1.100"]
        mock_prompt.assert_called_once_with(
            [("Over The Air (192.168.1.100)", "192.168.1.100")],
            purpose=None,
        )


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_no_defaults_with_api() -> None:
    """Test interactive mode with API option."""
    setup_core(config={"api": {}}, address="192.168.1.100")

    with patch(
        "esphome.__main__.choose_prompt", return_value="192.168.1.100"
    ) as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            show_ota=False,
            show_mqtt=False,
            show_api=True,
        )
        assert result == ["192.168.1.100"]
        mock_prompt.assert_called_once_with(
            [("Over The Air (192.168.1.100)", "192.168.1.100")],
            purpose=None,
        )


@pytest.mark.usefixtures("mock_no_serial_ports", "mock_has_mqtt_logging")
def test_choose_upload_log_host_no_defaults_with_mqtt() -> None:
    """Test interactive mode with MQTT option."""
    setup_core(config={CONF_MQTT: {CONF_BROKER: "mqtt.local"}})

    with patch("esphome.__main__.choose_prompt", return_value="MQTT") as mock_prompt:
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            show_ota=False,
            show_mqtt=True,
            show_api=False,
        )
        assert result == ["MQTT"]
        mock_prompt.assert_called_once_with(
            [("MQTT (mqtt.local)", "MQTT")],
            purpose=None,
        )


@pytest.mark.usefixtures("mock_has_mqtt_logging")
def test_choose_upload_log_host_no_defaults_with_all_options(
    mock_choose_prompt: Mock,
) -> None:
    """Test interactive mode with all options available."""
    setup_core(
        config={"ota": {}, "api": {}, CONF_MQTT: {CONF_BROKER: "mqtt.local"}},
        address="192.168.1.100",
    )

    mock_ports = [MockSerialPort("/dev/ttyUSB0", "USB Serial")]

    with patch("esphome.__main__.get_serial_ports", return_value=mock_ports):
        result = choose_upload_log_host(
            default=None,
            check_default=None,
            show_ota=True,
            show_mqtt=True,
            show_api=True,
            purpose="testing",
        )
        assert result == ["/dev/ttyUSB0"]

        expected_options = [
            ("/dev/ttyUSB0 (USB Serial)", "/dev/ttyUSB0"),
            ("Over The Air (192.168.1.100)", "192.168.1.100"),
            ("MQTT (mqtt.local)", "MQTT"),
        ]
        mock_choose_prompt.assert_called_once_with(expected_options, purpose="testing")


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_check_default_matches() -> None:
    """Test when check_default matches an available option."""
    setup_core(config={"ota": {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default=None,
        check_default="192.168.1.100",
        show_ota=True,
        show_mqtt=False,
        show_api=False,
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
            show_ota=False,
            show_mqtt=False,
            show_api=False,
        )
        assert result == ["fallback"]
        mock_prompt.assert_called_once()


@pytest.mark.usefixtures("mock_no_serial_ports")
def test_choose_upload_log_host_empty_defaults_list() -> None:
    """Test with an empty list as default."""
    with patch("esphome.__main__.choose_prompt", return_value="chosen") as mock_prompt:
        result = choose_upload_log_host(
            default=[],
            check_default=None,
            show_ota=False,
            show_mqtt=False,
            show_api=False,
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
        show_ota=False,
        show_mqtt=False,
        show_api=False,
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
        show_ota=False,
        show_mqtt=False,
        show_api=False,
    )
    assert result == ["192.168.1.50"]


def test_choose_upload_log_host_ota_both_conditions() -> None:
    """Test OTA device when both OTA and API are configured and enabled."""
    setup_core(config={"ota": {}, "api": {}}, address="192.168.1.100")

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        show_ota=True,
        show_mqtt=False,
        show_api=True,
    )
    assert result == ["192.168.1.100"]


@pytest.mark.usefixtures("mock_no_mqtt_logging")
def test_choose_upload_log_host_no_address_with_ota_config() -> None:
    """Test OTA device when OTA is configured but no address is set."""
    setup_core(config={"ota": {}})

    result = choose_upload_log_host(
        default="OTA",
        check_default=None,
        show_ota=True,
        show_mqtt=False,
        show_api=False,
    )
    assert result == []
