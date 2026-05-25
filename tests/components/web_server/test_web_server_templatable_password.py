"""Tests for templatable password in web_server component."""

import pytest

from esphome.components.web_server import CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_AUTH, CONF_PASSWORD, CONF_PORT, CONF_USERNAME


def test_web_server_password_static():
    """Test that static password works."""
    config = {
        CONF_PORT: 80,
        CONF_AUTH: {
            CONF_USERNAME: "admin",
            CONF_PASSWORD: "password123",
        },
    }
    result = CONFIG_SCHEMA(config)
    assert result[CONF_AUTH][CONF_PASSWORD] == "password123"


def test_web_server_password_templatable():
    """Test that templatable password (with lambda) is accepted."""
    config = {
        CONF_PORT: 80,
        CONF_AUTH: {
            CONF_USERNAME: "admin",
            CONF_PASSWORD: "!lambda 'return \"dynamic_password\";'",
        },
    }
    result = CONFIG_SCHEMA(config)
    assert CONF_AUTH in result
    assert CONF_PASSWORD in result[CONF_AUTH]


def test_web_server_password_empty():
    """Test that empty password is rejected."""
    config = {
        CONF_PORT: 80,
        CONF_AUTH: {
            CONF_USERNAME: "admin",
            CONF_PASSWORD: "",
        },
    }
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(config)


def test_web_server_without_auth():
    """Test that web_server works without auth."""
    config = {
        CONF_PORT: 80,
    }
    result = CONFIG_SCHEMA(config)
    assert CONF_AUTH not in result
