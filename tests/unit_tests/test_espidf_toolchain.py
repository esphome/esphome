"""Tests for esphome.espidf.toolchain helpers."""

# pylint: disable=protected-access

from unittest.mock import patch

from esphome.const import CONF_FRAMEWORK, CONF_SOURCE
from esphome.core import CORE
from esphome.espidf import toolchain


def test_get_framework_source_override_no_config():
    """When CORE.config hasn't been set, no override is returned."""
    CORE.config = None
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_no_esp32_section():
    """A config without an esp32 section yields no override."""
    CORE.config = {}
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_no_framework_source():
    """An esp32 section without framework.source yields no override."""
    CORE.config = {"esp32": {CONF_FRAMEWORK: {}}}
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_returns_value():
    """A user-supplied framework source is returned verbatim."""
    url = "https://example.com/esp-idf-v{VERSION}.tar.xz"
    CORE.config = {"esp32": {CONF_FRAMEWORK: {CONF_SOURCE: url}}}
    assert toolchain._get_framework_source_override() == url


def test_get_esphome_esp_idf_paths_forwards_source_override():
    """_get_esphome_esp_idf_paths threads the override into check_esp_idf_install."""
    url = "https://my-mirror/esp-idf-v{VERSION}.tar.xz"
    CORE.config = {"esp32": {CONF_FRAMEWORK: {CONF_SOURCE: url}}}
    # Hit a fresh cache key so check_esp_idf_install is actually called.
    toolchain._cache().paths.clear()
    with patch.object(
        toolchain, "check_esp_idf_install", return_value=("/fw", "/penv")
    ) as mock_install:
        toolchain._get_esphome_esp_idf_paths("5.5.4")
    mock_install.assert_called_once_with("5.5.4", source_url=url)


def test_get_esphome_esp_idf_paths_no_override():
    """When no source override is configured, source_url=None is passed."""
    CORE.config = {}
    toolchain._cache().paths.clear()
    with patch.object(
        toolchain, "check_esp_idf_install", return_value=("/fw", "/penv")
    ) as mock_install:
        toolchain._get_esphome_esp_idf_paths("5.5.4")
    mock_install.assert_called_once_with("5.5.4", source_url=None)
