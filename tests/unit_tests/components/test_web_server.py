"""Tests for web_server component helpers."""

import logging

import pytest

from esphome.components.web_server import (
    _final_validate_ap_only,
    serve_local,
    wifi_is_ap_only,
)
from esphome.const import (
    CONF_AP,
    CONF_LOCAL,
    CONF_MANUAL_IP,
    CONF_NETWORKS,
    CONF_SSID,
    CONF_STATIC_IP,
    CONF_VERSION,
    CONF_WIFI,
)
import esphome.final_validate as fv

AP_ONLY = {CONF_AP: {}}
AP_FALLBACK = {CONF_AP: {}, CONF_NETWORKS: [{CONF_SSID: "x"}]}
STA_ONLY = {CONF_NETWORKS: [{CONF_SSID: "x"}]}


@pytest.mark.parametrize(
    ("wifi_config", "expected"),
    [(AP_ONLY, True), (AP_FALLBACK, False), (STA_ONLY, False), (None, False)],
)
def test_wifi_is_ap_only(wifi_config: dict | None, expected: bool) -> None:
    assert wifi_is_ap_only(wifi_config) is expected


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
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_ONLY}, "http://192.168.4.1/"),
        (
            {CONF_VERSION: 2},
            {CONF_WIFI: {CONF_AP: {CONF_MANUAL_IP: {CONF_STATIC_IP: "10.0.0.1"}}}},
            "http://10.0.0.1/",
        ),
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_ONLY}, "not a captive portal"),
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_ONLY}, "embedded in the firmware"),
        ({CONF_VERSION: 2, CONF_LOCAL: False}, {CONF_WIFI: AP_ONLY}, "stays blank"),
        ({CONF_VERSION: 2}, {CONF_WIFI: AP_FALLBACK}, None),
        ({CONF_VERSION: 2}, {CONF_WIFI: STA_ONLY}, None),
        ({CONF_VERSION: 2}, {}, None),
    ],
)
def test_final_validate_ap_only_warning(
    web_server_config: dict,
    full_config: dict,
    expected: str | None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """AP only WiFi with web_server warns and names the AP address; others stay quiet."""
    token = fv.full_config.set({"web_server": web_server_config, **full_config})
    try:
        with caplog.at_level(logging.WARNING):
            _final_validate_ap_only(web_server_config)
    finally:
        fv.full_config.reset(token)
    if expected is None:
        assert "AP only" not in caplog.text
    else:
        assert expected in caplog.text


def test_final_validate_ap_only_with_captive_portal(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """With captive_portal the AP is captive, so that clause is left out."""
    token = fv.full_config.set(
        {"web_server": {CONF_VERSION: 2}, CONF_WIFI: AP_ONLY, "captive_portal": {}}
    )
    try:
        with caplog.at_level(logging.WARNING):
            _final_validate_ap_only({CONF_VERSION: 2})
    finally:
        fv.full_config.reset(token)
    assert "AP only" in caplog.text
    assert "captive portal" not in caplog.text
