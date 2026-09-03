"""Tests for AUTO_LOAD functionality including dynamic AUTO_LOAD."""

from pathlib import Path
from typing import Any
from unittest.mock import Mock

import pytest

from esphome import config, config_validation as cv, yaml_util
from esphome.core import CORE


@pytest.fixture
def fixtures_dir() -> Path:
    """Get the fixtures directory."""
    return Path(__file__).parent / "fixtures"


@pytest.fixture
def default_component() -> Mock:
    """Create a default mock component for unmocked components."""
    return Mock(
        auto_load=[],
        is_platform_component=False,
        is_platform=False,
        multi_conf=False,
        multi_conf_no_default=False,
        dependencies=[],
        conflicts_with=[],
        config_schema=cv.Schema({}, extra=cv.ALLOW_EXTRA),
    )


@pytest.fixture
def static_auto_load_component() -> Mock:
    """Create a mock component with static AUTO_LOAD."""
    return Mock(
        auto_load=["logger"],
        is_platform_component=False,
        is_platform=False,
        multi_conf=False,
        multi_conf_no_default=False,
        dependencies=[],
        conflicts_with=[],
        config_schema=cv.Schema({}, extra=cv.ALLOW_EXTRA),
    )


def test_static_auto_load_adds_components(
    mock_get_component: Mock,
    fixtures_dir: Path,
    static_auto_load_component: Mock,
    default_component: Mock,
) -> None:
    """Test that static AUTO_LOAD triggers loading of specified components."""
    CORE.config_path = fixtures_dir / "auto_load_static.yaml"

    config_file = fixtures_dir / "auto_load_static.yaml"
    raw_config = yaml_util.load_yaml(config_file)

    component_mocks = {"test_component": static_auto_load_component}
    mock_get_component.side_effect = lambda name: component_mocks.get(
        name, default_component
    )

    result = config.validate_config(raw_config, {})

    # Check for validation errors
    assert not result.errors, f"Validation errors: {result.errors}"

    # Logger should have been auto-loaded by test_component
    assert "logger" in result
    assert "test_component" in result


def test_dynamic_auto_load_with_config_param(
    mock_get_component: Mock,
    fixtures_dir: Path,
    default_component: Mock,
) -> None:
    """Test that dynamic AUTO_LOAD evaluates based on configuration."""
    CORE.config_path = fixtures_dir / "auto_load_dynamic.yaml"

    config_file = fixtures_dir / "auto_load_dynamic.yaml"
    raw_config = yaml_util.load_yaml(config_file)

    # Track if auto_load was called with config
    auto_load_calls = []

    def dynamic_auto_load(conf: dict[str, Any]) -> list[str]:
        """Dynamically load components based on config."""
        auto_load_calls.append(conf)
        component_map = {
            "enable_logger": "logger",
            "enable_api": "api",
        }
        return [comp for key, comp in component_map.items() if conf.get(key)]

    dynamic_component = Mock(
        auto_load=dynamic_auto_load,
        is_platform_component=False,
        is_platform=False,
        multi_conf=False,
        multi_conf_no_default=False,
        dependencies=[],
        conflicts_with=[],
        config_schema=cv.Schema({}, extra=cv.ALLOW_EXTRA),
    )

    component_mocks = {"test_component": dynamic_component}
    mock_get_component.side_effect = lambda name: component_mocks.get(
        name, default_component
    )

    result = config.validate_config(raw_config, {})

    # Check for validation errors
    assert not result.errors, f"Validation errors: {result.errors}"

    # Verify auto_load was called with the validated config
    assert len(auto_load_calls) == 1, "auto_load should be called exactly once"
    assert auto_load_calls[0].get("enable_logger") is True
    assert auto_load_calls[0].get("enable_api") is False

    # Only logger should be auto-loaded (enable_logger=true in YAML)
    assert "logger" in result, (
        f"Logger not found in result. Result keys: {list(result.keys())}"
    )
    # API should NOT be auto-loaded (enable_api=false in YAML)
    assert "api" not in result
    assert "test_component" in result
