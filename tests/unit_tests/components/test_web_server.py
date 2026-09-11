"""Tests for the web_server AP mode helpers."""

import logging

import pytest

from esphome.components.web_server import (
    _final_validate_ap_mode,
    serve_captive,
    serve_local,
)
from esphome.const import (
    CONF_AP,
    CONF_LOCAL,
    CONF_NETWORKS,
    CONF_PORT,
    CONF_SSID,
    CONF_VERSION,
    CONF_WIFI,
)
import esphome.final_validate as fv

AP_ONLY = {CONF_AP: {}}
AP_FALLBACK = {CONF_AP: {}, CONF_NETWORKS: [{CONF_SSID: "x"}]}
STA_ONLY = {CONF_NETWORKS: [{CONF_SSID: "x"}]}


@pytest.mark.parametrize(
    ("web_server_config", "wifi_config", "expected"),
    [
        # AP only: embed the interface, the AP has no internet.
        ({CONF_VERSION: 2}, AP_ONLY, True),
        ({CONF_VERSION: 3}, AP_ONLY, True),
        # Explicit setting always wins.
        ({CONF_VERSION: 2, CONF_LOCAL: False}, AP_ONLY, False),
        ({CONF_VERSION: 2, CONF_LOCAL: True}, STA_ONLY, True),
        # AP fallback, no AP, no wifi, or version 1 (no local mode): hosted page.
        ({CONF_VERSION: 2}, AP_FALLBACK, False),
        ({CONF_VERSION: 2}, STA_ONLY, False),
        ({CONF_VERSION: 2}, None, False),
        ({CONF_VERSION: 1}, AP_ONLY, False),
    ],
)
def test_serve_local(
    web_server_config: dict, wifi_config: dict | None, expected: bool
) -> None:
    """The interface is embedded for AP only WiFi unless local is set explicitly."""
    assert serve_local(web_server_config, wifi_config) is expected


@pytest.mark.parametrize(
    ("web_server_config", "full_config", "expected"),
    [
        # AP only: local is implied, web_server is the captive portal.
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_ONLY}, True),
        # Captive portal probes only work on port 80.
        ({CONF_VERSION: 2, CONF_PORT: 8080}, {CONF_WIFI: AP_ONLY}, False),
        # AP fallback needs an explicit local: true to be captive.
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_FALLBACK}, False),
        ({CONF_VERSION: 2, CONF_LOCAL: True}, {CONF_WIFI: AP_FALLBACK}, True),
        # captive_portal owns the role when configured.
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_ONLY, "captive_portal": {}}, False),
        # No AP, no wifi, hosted page, or version 1: never captive.
        ({CONF_VERSION: 2, CONF_LOCAL: True}, {CONF_WIFI: STA_ONLY}, False),
        ({CONF_VERSION: 2, CONF_LOCAL: True}, {}, False),
        ({CONF_VERSION: 2, CONF_LOCAL: False}, {CONF_WIFI: AP_ONLY}, False),
        ({CONF_VERSION: 1}, {CONF_WIFI: AP_ONLY}, False),
    ],
)
def test_serve_captive(
    web_server_config: dict, full_config: dict, expected: bool
) -> None:
    web_server_config.setdefault(CONF_PORT, 80)
    assert serve_captive(web_server_config, full_config) is expected


@pytest.mark.parametrize(
    ("web_server_config", "expect_warning"),
    [
        # Explicit local: false on an AP only device: the hosted page will stay blank.
        ({CONF_VERSION: 2, CONF_PORT: 80, CONF_LOCAL: False}, True),
        # Default: embedded and captive, nothing to warn about.
        ({CONF_VERSION: 2, CONF_PORT: 80}, False),
    ],
)
def test_final_validate_ap_mode_warns_for_hosted_page(
    web_server_config: dict, expect_warning: bool, caplog: pytest.LogCaptureFixture
) -> None:
    token = fv.full_config.set({"web_server": web_server_config, CONF_WIFI: AP_ONLY})
    try:
        with caplog.at_level(logging.WARNING):
            _final_validate_ap_mode(web_server_config)
    finally:
        fv.full_config.reset(token)
    assert ("stays blank" in caplog.text) is expect_warning


def test_final_validate_ap_mode_warns_for_non_default_port(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Captive portal detection needs port 80; other ports get a hint, not captive mode."""
    config = {CONF_VERSION: 2, CONF_PORT: 8080}
    token = fv.full_config.set({"web_server": config, CONF_WIFI: AP_ONLY})
    try:
        with caplog.at_level(logging.WARNING):
            _final_validate_ap_mode(config)
    finally:
        fv.full_config.reset(token)
    assert "cannot open automatically" in caplog.text
    assert "http://192.168.4.1:8080/" in caplog.text


def test_final_validate_ap_mode_port_warning_uses_manual_ip(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The manual URL in the port warning honors wifi.ap.manual_ip."""
    from esphome.const import CONF_MANUAL_IP, CONF_STATIC_IP

    config = {CONF_VERSION: 2, CONF_PORT: 8080}
    wifi = {CONF_AP: {CONF_MANUAL_IP: {CONF_STATIC_IP: "10.0.0.1"}}}
    token = fv.full_config.set({"web_server": config, CONF_WIFI: wifi})
    try:
        with caplog.at_level(logging.WARNING):
            _final_validate_ap_mode(config)
    finally:
        fv.full_config.reset(token)
    assert "http://10.0.0.1:8080/" in caplog.text


@pytest.mark.parametrize(
    ("wifi_config", "expected"),
    [
        # Explicit local: true on a fallback AP: announce the captive fallback role.
        (AP_FALLBACK, "fallback access point"),
        # Explicit local: true on AP only skips the implied-local info; still announce.
        (AP_ONLY, "captive portal while the access point"),
    ],
)
def test_final_validate_ap_mode_informs_explicit_local_captive(
    wifi_config: dict, expected: str, caplog: pytest.LogCaptureFixture
) -> None:
    """Explicit local: true logs that web_server becomes the captive portal."""
    config = {CONF_VERSION: 2, CONF_PORT: 80, CONF_LOCAL: True}
    token = fv.full_config.set({"web_server": config, CONF_WIFI: wifi_config})
    try:
        with caplog.at_level(logging.INFO):
            _final_validate_ap_mode(config)
    finally:
        fv.full_config.reset(token)
    assert expected in caplog.text
