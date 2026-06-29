"""Unit tests for esphome.config module."""

from collections.abc import Generator
import logging
from pathlib import Path
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome import config, yaml_util
from esphome.core import CORE


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

    for domain, _component, conf in configs:
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


def _write_merge_conflict_config(tmp_path: Path, *, suppress: bool) -> Path:
    """Create a config where two `<<` includes both define `logger:`.

    The second `logger:` is dropped by the shallow merge. Returns the main file.
    """
    (tmp_path / "a.yaml").write_text("logger:\n  level: DEBUG\n")
    (tmp_path / "b.yaml").write_text("logger:\n  level: INFO\n")
    esphome_section = "esphome:\n  name: test\n"
    if suppress:
        esphome_section += "  merge_warnings: false\n"
    main = tmp_path / "main.yaml"
    main.write_text(f"{esphome_section}<<: !include a.yaml\n<<: !include b.yaml\n")
    return main


def test_validate_config_warns_on_dropped_merge_key(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """By default, a `<<` merge that drops a key logs a warning."""
    main = _write_merge_conflict_config(tmp_path, suppress=False)
    CORE.config_path = main
    raw_config = yaml_util.load_yaml(main)

    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        config.validate_config(raw_config, {})

    assert any(
        "was dropped while processing a '<<' merge" in record.message
        and "logger" in record.message
        for record in caplog.records
    )
    # The queue is drained so the warning cannot leak into a later run.
    assert yaml_util.take_dropped_merge_keys() == []


def test_validate_config_suppresses_merge_warning(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """`esphome: merge_warnings: false` hides the warning but still drains the queue."""
    main = _write_merge_conflict_config(tmp_path, suppress=True)
    CORE.config_path = main
    raw_config = yaml_util.load_yaml(main)

    with caplog.at_level(logging.WARNING, logger="esphome.config"):
        config.validate_config(raw_config, {})

    assert not any(
        "was dropped while processing a '<<' merge" in record.message
        for record in caplog.records
    )
    # The queue is drained even when the warning is suppressed.
    assert yaml_util.take_dropped_merge_keys() == []
