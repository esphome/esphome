"""Unit tests for esphome.__main__ module."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome.__main__ import choose_upload_log_host, show_logs, upload_program
from esphome.const import (
    CONF_BROKER,
    CONF_DISABLED,
    CONF_ESPHOME,
    CONF_MDNS,
    CONF_MQTT,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
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


@dataclass
class MockArgs:
    """Mock args for testing."""

    file: str | None = None
    upload_speed: int = 460800
    username: str | None = None
    password: str | None = None
    client_id: str | None = None
    topic: str | None = None


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
    expected_firmware = str(
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
    mock_run_ota.assert_called_once_with(["192.168.1.100"], 3232, "", "custom.bin")


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


@patch("esphome.mqtt.get_esphome_device_ip")
def test_upload_program_ota_with_mqtt_resolution(
    mock_mqtt_get_ip: Mock,
    mock_is_ip_address: Mock,
    mock_run_ota: Mock,
    mock_get_port_type: Mock,
    tmp_path: Path,
) -> None:
    """Test upload_program with OTA using MQTT for address resolution."""
    setup_core(address="device.local", platform=PLATFORM_ESP32, tmp_path=tmp_path)

    mock_get_port_type.side_effect = ["MQTT", "NETWORK"]
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
    expected_firmware = str(
        tmp_path / ".esphome" / "build" / "test" / ".pioenvs" / "test" / "firmware.bin"
    )
    mock_run_ota.assert_called_once_with(
        [["192.168.1.100"]], 3232, "", expected_firmware
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
    mock_get_port_type: Mock,
) -> None:
    """Test show_logs with API."""
    setup_core(
        config={
            "logger": {},
            "api": {},
            CONF_MDNS: {CONF_DISABLED: False},
        },
        platform=PLATFORM_ESP32,
    )
    mock_get_port_type.return_value = "NETWORK"
    mock_run_logs.return_value = 0

    args = MockArgs()
    devices = ["192.168.1.100", "192.168.1.101"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_run_logs.assert_called_once_with(
        CORE.config, ["192.168.1.100", "192.168.1.101"]
    )


@patch("esphome.mqtt.get_esphome_device_ip")
@patch("esphome.components.api.client.run_logs")
def test_show_logs_api_with_mqtt_fallback(
    mock_run_logs: Mock,
    mock_mqtt_get_ip: Mock,
    mock_get_port_type: Mock,
) -> None:
    """Test show_logs with API using MQTT for address resolution."""
    setup_core(
        config={
            "logger": {},
            "api": {},
            CONF_MDNS: {CONF_DISABLED: True},
            CONF_MQTT: {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_get_port_type.return_value = "NETWORK"
    mock_run_logs.return_value = 0
    mock_mqtt_get_ip.return_value = ["192.168.1.200"]

    args = MockArgs(username="user", password="pass", client_id="client")
    devices = ["device.local"]

    result = show_logs(CORE.config, args, devices)

    assert result == 0
    mock_mqtt_get_ip.assert_called_once_with(CORE.config, "user", "pass", "client")
    mock_run_logs.assert_called_once_with(CORE.config, ["192.168.1.200"])


@patch("esphome.mqtt.show_logs")
def test_show_logs_mqtt(
    mock_mqtt_show_logs: Mock,
    mock_get_port_type: Mock,
) -> None:
    """Test show_logs with MQTT."""
    setup_core(
        config={
            "logger": {},
            "mqtt": {CONF_BROKER: "mqtt.local"},
        },
        platform=PLATFORM_ESP32,
    )
    mock_get_port_type.return_value = "MQTT"
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
    mock_get_port_type: Mock,
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
    mock_get_port_type.return_value = "NETWORK"
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


def test_show_logs_no_method_configured(
    mock_get_port_type: Mock,
) -> None:
    """Test show_logs when no remote logging method is configured."""
    setup_core(
        config={
            "logger": {},
            # No API or MQTT configured
        },
        platform=PLATFORM_ESP32,
    )
    mock_get_port_type.return_value = "NETWORK"

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
