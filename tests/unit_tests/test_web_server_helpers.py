"""Unit tests for esphome.web_server_helpers module."""

from __future__ import annotations

import socket

import pytest

from esphome.const import (
    CONF_AUTH,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_USERNAME,
    CONF_WEB_SERVER,
)
from esphome.core import EsphomeError
from esphome.web_server_helpers import (
    get_web_server_connection,
    resolve_web_server_urls,
)


def test_resolve_web_server_urls_maps_ipv4_and_ipv6(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Each resolved address becomes an (ip, url) pair with IPv6 bracketing."""
    addr_infos = [
        (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.5", 80)),
        (socket.AF_INET6, socket.SOCK_STREAM, 0, "", ("fe80::1", 80, 0, 7)),
    ]
    monkeypatch.setattr(
        "esphome.web_server_helpers.resolve_ip_address",
        lambda *args, **kwargs: addr_infos,
    )

    assert resolve_web_server_urls("dev.local", 80, "/events") == [
        ("192.168.1.5", "http://192.168.1.5:80/events"),
        ("fe80::1", "http://[fe80::1%257]:80/events"),
    ]


def test_get_web_server_connection_without_auth() -> None:
    """Port is returned and credentials are None when no auth is configured."""
    config = {CONF_WEB_SERVER: {CONF_PORT: 80}}

    assert get_web_server_connection(config) == (80, None, None)


def test_get_web_server_connection_with_auth() -> None:
    """Port and HTTP Basic credentials are returned when auth is configured."""
    config = {
        CONF_WEB_SERVER: {
            CONF_PORT: 8080,
            CONF_AUTH: {CONF_USERNAME: "admin", CONF_PASSWORD: "secret"},
        }
    }

    assert get_web_server_connection(config) == (8080, "admin", "secret")


def test_get_web_server_connection_missing_component() -> None:
    """A config without web_server raises a clear error."""
    with pytest.raises(EsphomeError, match="web_server.*not configured"):
        get_web_server_connection({})
