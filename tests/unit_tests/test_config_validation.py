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


def test_ota_component_configs_with_proper_platform_list(
    mock_get_component: Mock,
    mock_get_platform: Mock,
) -> None:
    """Test iter_component_configs handles OTA properly configured as a list."""
    test_config = {
        "ota": [
            {"platform": "esphome", "password": "test123", "id": "my_ota"},
        ],
    }

    mock_get_component.return_value = MagicMock(
        is_platform_component=True, multi_conf=False
    )

    configs = list(config.iter_component_configs(test_config))
    assert len(configs) == 2

    assert configs[0][0] == "ota"
    assert configs[0][2] == test_config["ota"]  # The list itself

    assert configs[1][0] == "ota.esphome"
    assert configs[1][2]["platform"] == "esphome"
    assert configs[1][2]["password"] == "test123"


def test_iter_component_configs_with_multi_conf(mock_get_component: Mock) -> None:
    """Test that iter_component_configs handles multi_conf components correctly."""
    test_config = {
        "switch": [
            {"name": "Switch 1"},
            {"name": "Switch 2"},
        ],
    }

    mock_get_component.return_value = MagicMock(
        is_platform_component=False, multi_conf=True
    )

    configs = list(config.iter_component_configs(test_config))
    assert len(configs) == 2

    for domain, component, conf in configs:
        assert domain == "switch"
        assert "name" in conf


def test_ota_no_platform_with_captive_portal(fixtures_dir: Path) -> None:
    """Test OTA with no platform (ota:) gets normalized when captive_portal auto-loads."""
    CORE.config_path = fixtures_dir / "dummy.yaml"

    config_file = fixtures_dir / "ota_no_platform.yaml"
    raw_config = yaml_util.load_yaml(config_file)
    result = config.validate_config(raw_config, {})

    assert "ota" in result
    assert isinstance(result["ota"], list), f"Expected list, got {type(result['ota'])}"
    platforms = {p.get("platform") for p in result["ota"]}
    assert "web_server" in platforms, f"Expected web_server platform in {platforms}"


def test_ota_empty_dict_with_captive_portal(fixtures_dir: Path) -> None:
    """Test OTA with empty dict ({}) gets normalized when captive_portal auto-loads."""
    CORE.config_path = fixtures_dir / "dummy.yaml"

    config_file = fixtures_dir / "ota_empty_dict.yaml"
    raw_config = yaml_util.load_yaml(config_file)
    result = config.validate_config(raw_config, {})

    assert "ota" in result
    assert isinstance(result["ota"], list), f"Expected list, got {type(result['ota'])}"
    platforms = {p.get("platform") for p in result["ota"]}
    assert "web_server" in platforms, f"Expected web_server platform in {platforms}"


def test_ota_with_platform_list_and_captive_portal(fixtures_dir: Path) -> None:
    """Test OTA with proper platform list remains valid when captive_portal auto-loads."""
    CORE.config_path = fixtures_dir / "dummy.yaml"

    config_file = fixtures_dir / "ota_with_platform_list.yaml"
    raw_config = yaml_util.load_yaml(config_file)
    result = config.validate_config(raw_config, {})

    assert "ota" in result
    assert isinstance(result["ota"], list), f"Expected list, got {type(result['ota'])}"
    platforms = {p.get("platform") for p in result["ota"]}
    assert "esphome" in platforms, f"Expected esphome platform in {platforms}"
    assert "web_server" in platforms, f"Expected web_server platform in {platforms}"
