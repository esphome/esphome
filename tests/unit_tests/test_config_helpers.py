"""Unit tests for esphome.config_helpers module."""

from collections.abc import Callable
from unittest.mock import patch

from esphome.config_helpers import filter_source_files_from_platform, get_logger_level
from esphome.const import (
    CONF_LEVEL,
    CONF_LOGGER,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PlatformFramework,
)


def test_filter_source_files_from_platform_esp32() -> None:
    """Test that filter_source_files_from_platform correctly filters files for ESP32 platform."""
    # Define test file mappings
    files_map: dict[str, set[PlatformFramework]] = {
        "logger_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "logger_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "logger_host.cpp": {PlatformFramework.HOST_NATIVE},
        "logger_common.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.HOST_NATIVE,
        },
    }

    # Create the filter function
    filter_func: Callable[[], list[str]] = filter_source_files_from_platform(files_map)

    # Test ESP32 with Arduino framework
    mock_core_data: dict[str, dict[str, str]] = {
        KEY_CORE: {
            KEY_TARGET_PLATFORM: "esp32",
            KEY_TARGET_FRAMEWORK: "arduino",
        }
    }

    with patch("esphome.config_helpers.CORE.data", mock_core_data):
        excluded: list[str] = filter_func()
        # ESP32 Arduino should exclude ESP8266 and HOST files
        assert "logger_esp8266.cpp" in excluded
        assert "logger_host.cpp" in excluded
        # But not ESP32 or common files
        assert "logger_esp32.cpp" not in excluded
        assert "logger_common.cpp" not in excluded


def test_filter_source_files_from_platform_host() -> None:
    """Test that filter_source_files_from_platform correctly filters files for HOST platform."""
    # Define test file mappings
    files_map: dict[str, set[PlatformFramework]] = {
        "logger_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "logger_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "logger_host.cpp": {PlatformFramework.HOST_NATIVE},
        "logger_common.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.HOST_NATIVE,
        },
    }

    # Create the filter function
    filter_func: Callable[[], list[str]] = filter_source_files_from_platform(files_map)

    # Test Host platform
    mock_core_data: dict[str, dict[str, str]] = {
        KEY_CORE: {
            KEY_TARGET_PLATFORM: "host",
            KEY_TARGET_FRAMEWORK: "host",  # Framework.NATIVE is "host"
        }
    }

    with patch("esphome.config_helpers.CORE.data", mock_core_data):
        excluded: list[str] = filter_func()
        # Host should exclude ESP32 and ESP8266 files
        assert "logger_esp32.cpp" in excluded
        assert "logger_esp8266.cpp" in excluded
        # But not host or common files
        assert "logger_host.cpp" not in excluded
        assert "logger_common.cpp" not in excluded


def test_filter_source_files_from_platform_handles_missing_data() -> None:
    """Test that filter_source_files_from_platform returns empty list when platform/framework data is missing."""
    # Define test file mappings
    files_map: dict[str, set[PlatformFramework]] = {
        "logger_esp32.cpp": {PlatformFramework.ESP32_ARDUINO},
        "logger_host.cpp": {PlatformFramework.HOST_NATIVE},
    }

    # Create the filter function
    filter_func: Callable[[], list[str]] = filter_source_files_from_platform(files_map)

    # Test case: Missing platform/framework data
    mock_core_data: dict[str, dict[str, str]] = {KEY_CORE: {}}

    with patch("esphome.config_helpers.CORE.data", mock_core_data):
        excluded: list[str] = filter_func()
        # Should return empty list when platform/framework not set
        assert excluded == []


def test_get_logger_level() -> None:
    """Test get_logger_level helper function."""
    # Test no logger config - should return default DEBUG
    mock_config = {}
    with patch("esphome.config_helpers.CORE.config", mock_config):
        assert get_logger_level() == "DEBUG"

    # Test with logger set to INFO
    mock_config = {CONF_LOGGER: {CONF_LEVEL: "INFO"}}
    with patch("esphome.config_helpers.CORE.config", mock_config):
        assert get_logger_level() == "INFO"

    # Test with VERY_VERBOSE
    mock_config = {CONF_LOGGER: {CONF_LEVEL: "VERY_VERBOSE"}}
    with patch("esphome.config_helpers.CORE.config", mock_config):
        assert get_logger_level() == "VERY_VERBOSE"

    # Test with logger missing level (uses default DEBUG)
    mock_config = {CONF_LOGGER: {}}
    with patch("esphome.config_helpers.CORE.config", mock_config):
        assert get_logger_level() == "DEBUG"
