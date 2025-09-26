"""Unit tests for esphome.config module."""

from collections.abc import Generator
from pathlib import Path
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome import config, yaml_util
from esphome.core import CORE


@pytest.fixture
def mock_get_component() -> Generator[Mock, None, None]:
    """Fixture for mocking get_component."""
    with patch("esphome.config.get_component") as mock_get_component:
        yield mock_get_component


@pytest.fixture
def mock_get_platform() -> Generator[Mock, None, None]:
    """Fixture for mocking get_platform."""
    with patch("esphome.config.get_platform") as mock_get_platform:
        # Default mock platform
        mock_get_platform.return_value = MagicMock()
        yield mock_get_platform


@pytest.fixture
def fixtures_dir() -> Path:
    """Get the fixtures directory."""
    return Path(__file__).parent / "fixtures"


def test_iter_components_handles_non_list_platform_component(
    mock_get_component: Mock,
) -> None:
    """Test that iter_components handles platform components that have been normalized to empty list."""
    # After LoadValidationStep normalization, platform components without config
    # are converted to empty list
    test_config = {
        "ota": [],  # Normalized from None/dict to empty list by LoadValidationStep
        "wifi": {"ssid": "test"},
    }

    # Set up mock components
    components = {
        "ota": MagicMock(is_platform_component=True),
        "wifi": MagicMock(is_platform_component=False),
    }

    mock_get_component.side_effect = lambda domain: components.get(
        domain, MagicMock(is_platform_component=False)
    )

    # This should not raise TypeError
    components_list = list(config.iter_components(test_config))

    # Verify we got the expected components
    assert len(components_list) == 2  # ota and wifi (ota has no platforms)


def test_iter_component_configs_handles_non_list_platform_component(
    mock_get_component: Mock,
    mock_get_platform: Mock,
) -> None:
    """Test that iter_component_configs handles platform components that have been normalized."""

    # After LoadValidationStep normalization
    test_config = {
        "ota": [],  # Normalized from None/dict to empty list by LoadValidationStep
        "one_wire": [  # List config for platform component
            {"platform": "gpio", "pin": 10}
        ],
    }

    # Set up mock components
    components: dict[str, Mock] = {
        "ota": MagicMock(is_platform_component=True, multi_conf=False),
        "one_wire": MagicMock(is_platform_component=True, multi_conf=False),
    }

    # Default mock for unknown components
    default_mock = MagicMock(is_platform_component=False, multi_conf=False)

    mock_get_component.side_effect = lambda domain: components.get(domain, default_mock)

    # This should not raise TypeError
    configs = list(config.iter_component_configs(test_config))

    # Should have 3 items: ota (empty list), one_wire, and one_wire.gpio
    assert len(configs) == 3

    # Check the domains
    domains = [c[0] for c in configs]
    assert "ota" in domains
    assert "one_wire" in domains
    assert "one_wire.gpio" in domains


def test_iter_components_with_valid_platform_list(
    mock_get_component: Mock,
    mock_get_platform: Mock,
) -> None:
    """Test that iter_components works correctly with valid platform component list."""

    # Create a mock component that is a platform component
    mock_component = MagicMock()
    mock_component.is_platform_component = True

    # Create test config with proper list format
    test_config = {
        "sensor": [
            {"platform": "dht", "pin": 5},
            {"platform": "bme280", "address": 0x76},
        ],
    }

    mock_get_component.return_value = mock_component

    # Get all components
    components = list(config.iter_components(test_config))

    # Should have 3 items: sensor, sensor.dht, sensor.bme280
    assert len(components) == 3

    # Check the domains
    domains = [c[0] for c in components]
    assert "sensor" in domains
    assert "sensor.dht" in domains
    assert "sensor.bme280" in domains


def test_ota_with_proper_platform_list(
    mock_get_component: Mock,
    mock_get_platform: Mock,
) -> None:
    """Test that OTA component works correctly when configured as a list with platforms."""

    # Create test config where ota is properly configured as a list
    test_config = {
        "ota": [
            {"platform": "esphome", "password": "test123"},
        ],
    }

    mock_get_component.return_value = MagicMock(is_platform_component=True)

    # This should work without TypeError
    components = list(config.iter_components(test_config))

    # Should have 2 items: ota and ota.esphome
    assert len(components) == 2

    # Check the domains
    domains = [c[0] for c in components]
    assert "ota" in domains
    assert "ota.esphome" in domains


def test_ota_component_configs_with_proper_platform_list(
    mock_get_component: Mock,
    mock_get_platform: Mock,
) -> None:
    """Test that iter_component_configs handles OTA properly configured as a list."""

    # Create test config where ota is properly configured as a list
    test_config = {
        "ota": [
            {"platform": "esphome", "password": "test123", "id": "my_ota"},
        ],
    }

    mock_get_component.return_value = MagicMock(
        is_platform_component=True, multi_conf=False
    )

    # This should work without TypeError
    configs = list(config.iter_component_configs(test_config))

    # Should have 2 items: ota config and ota.esphome platform config
    assert len(configs) == 2

    # Check the domains and configs
    assert configs[0][0] == "ota"
    assert configs[0][2] == test_config["ota"]  # The list itself

    assert configs[1][0] == "ota.esphome"
    assert configs[1][2]["platform"] == "esphome"
    assert configs[1][2]["password"] == "test123"


def test_iter_component_configs_with_multi_conf(mock_get_component: Mock) -> None:
    """Test that iter_component_configs handles multi_conf components correctly."""
    # Create test config
    test_config = {
        "switch": [
            {"name": "Switch 1"},
            {"name": "Switch 2"},
        ],
    }

    # Set up mock component with multi_conf
    mock_get_component.return_value = MagicMock(
        is_platform_component=False, multi_conf=True
    )

    # Get all configs
    configs = list(config.iter_component_configs(test_config))

    # Should have 2 items (one for each switch)
    assert len(configs) == 2

    # Both should be for "switch" domain
    for domain, component, conf in configs:
        assert domain == "switch"
        assert "name" in conf


def test_ota_no_platform_with_captive_portal(fixtures_dir: Path) -> None:
    """Test OTA with no platform (ota:) gets normalized when captive_portal auto-loads."""
    # Set up CORE config path
    CORE.config_path = fixtures_dir / "dummy.yaml"

    # Load config with OTA having no value (ota:)
    config_file = fixtures_dir / "ota_no_platform.yaml"
    raw_config = yaml_util.load_yaml(config_file)
    result = config.validate_config(raw_config, {})

    # Check that OTA was normalized to a list and captive_portal added web_server
    assert "ota" in result
    assert isinstance(result["ota"], list), f"Expected list, got {type(result['ota'])}"
    # After captive_portal auto-loads, OTA should have web_server platform
    platforms = {p.get("platform") for p in result["ota"]}
    assert "web_server" in platforms, f"Expected web_server platform in {platforms}"


def test_ota_empty_dict_with_captive_portal(fixtures_dir: Path) -> None:
    """Test OTA with empty dict ({}) gets normalized when captive_portal auto-loads."""
    # Set up CORE config path
    CORE.config_path = fixtures_dir / "dummy.yaml"

    # Load config with OTA having empty dict (ota: {})
    config_file = fixtures_dir / "ota_empty_dict.yaml"
    raw_config = yaml_util.load_yaml(config_file)
    result = config.validate_config(raw_config, {})

    # Check that OTA was normalized to a list and captive_portal added web_server
    assert "ota" in result
    assert isinstance(result["ota"], list), f"Expected list, got {type(result['ota'])}"
    # The empty dict gets normalized and web_server is added
    platforms = {p.get("platform") for p in result["ota"]}
    assert "web_server" in platforms, f"Expected web_server platform in {platforms}"


def test_ota_with_platform_list_and_captive_portal(fixtures_dir: Path) -> None:
    """Test OTA with proper platform list remains valid when captive_portal auto-loads."""
    # Set up CORE config path
    CORE.config_path = fixtures_dir / "dummy.yaml"

    # Load config with OTA having proper list format
    config_file = fixtures_dir / "ota_with_platform_list.yaml"
    raw_config = yaml_util.load_yaml(config_file)
    result = config.validate_config(raw_config, {})

    # Check that OTA remains a list with both esphome and web_server platforms
    assert "ota" in result
    assert isinstance(result["ota"], list), f"Expected list, got {type(result['ota'])}"
    platforms = {p.get("platform") for p in result["ota"]}
    assert "esphome" in platforms, f"Expected esphome platform in {platforms}"
    assert "web_server" in platforms, f"Expected web_server platform in {platforms}"
