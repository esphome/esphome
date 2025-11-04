"""Tests for web_server OTA migration validation."""

import pytest

from esphome import config_validation as cv
from esphome.types import ConfigType


def test_web_server_ota_true_fails_validation() -> None:
    """Test that web_server with ota: true fails validation with helpful message."""
    from esphome.components.web_server import validate_ota

    # Config with ota: true should fail
    config: ConfigType = {"ota": True}

    with pytest.raises(cv.Invalid) as exc_info:
        validate_ota(config)

    # Check error message contains migration instructions
    error_msg = str(exc_info.value)
    assert "only accepts 'false' to disable OTA" in error_msg
    assert "platform: web_server" in error_msg
    assert "ota:" in error_msg


def test_web_server_ota_false_passes_validation() -> None:
    """Test that web_server with ota: false passes validation."""
    from esphome.components.web_server import validate_ota

    # Config with ota: false should pass
    config: ConfigType = {"ota": False}
    result = validate_ota(config)
    assert result == config

    # Config without ota should also pass
    config: ConfigType = {}
    result = validate_ota(config)
    assert result == config
