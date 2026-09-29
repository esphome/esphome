"""Tests for web_server Private Network Access / allowed_origins validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.web_server import (
    CONF_ALLOWED_ORIGINS,
    validate_origin,
    validate_private_network_access,
)
from esphome.const import CONF_ENABLE_PRIVATE_NETWORK_ACCESS
from esphome.types import ConfigType


def test_pna_enabled_without_origins_fails() -> None:
    """Enabling PNA without allowed_origins must fail validation."""
    config: ConfigType = {CONF_ENABLE_PRIVATE_NETWORK_ACCESS: True}

    with pytest.raises(cv.Invalid) as exc_info:
        validate_private_network_access(config)

    error_msg = str(exc_info.value)
    assert CONF_ALLOWED_ORIGINS in error_msg
    assert "must be set" in error_msg


def test_pna_enabled_with_origins_passes() -> None:
    """Enabling PNA with at least one allowed origin passes validation."""
    config: ConfigType = {
        CONF_ENABLE_PRIVATE_NETWORK_ACCESS: True,
        CONF_ALLOWED_ORIGINS: ["https://app.esphome.io"],
    }
    assert validate_private_network_access(config) == config


def test_origins_without_pna_passes() -> None:
    """allowed_origins can be set without enabling PNA (they are independent)."""
    config: ConfigType = {
        CONF_ENABLE_PRIVATE_NETWORK_ACCESS: False,
        CONF_ALLOWED_ORIGINS: ["https://app.esphome.io"],
    }
    assert validate_private_network_access(config) == config


def test_pna_disabled_without_origins_passes() -> None:
    """PNA disabled and no origins specified passes validation."""
    config: ConfigType = {CONF_ENABLE_PRIVATE_NETWORK_ACCESS: False}
    assert validate_private_network_access(config) == config


def test_validate_origin_wildcard() -> None:
    """The '*' wildcard is accepted as-is."""
    assert validate_origin("*") == "*"


@pytest.mark.parametrize(
    "value",
    [
        "https://example.com",
        "http://example.com:8080",
        "https://192.168.1.5",
    ],
)
def test_validate_origin_valid(value: str) -> None:
    """Well-formed origins pass through unchanged."""
    assert validate_origin(value) == value


def test_validate_origin_lowercased() -> None:
    """Scheme and host are normalized to lowercase to match the browser Origin header."""
    assert validate_origin("HTTPS://App.Example.com") == "https://app.example.com"


@pytest.mark.parametrize(
    "value",
    [
        "https://example.com/",  # trailing slash
        "https://example.com/path",  # path segment
        "example.com",  # missing scheme
        "",  # empty
    ],
)
def test_validate_origin_invalid(value: str) -> None:
    """Malformed origins are rejected at config time instead of silently 403ing."""
    with pytest.raises(cv.Invalid, match="not a valid origin"):
        validate_origin(value)
