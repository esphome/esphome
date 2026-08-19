"""Tests for the web_server AP mode helpers."""

import logging

import pytest

from esphome.components.web_server import (
    _final_validate_ap_mode,
    serve_captive,
    serve_local,
    wifi_is_ap_only,
)
from esphome.const import (
    CONF_AP,
    CONF_LOCAL,
    CONF_NETWORKS,
    CONF_SSID,
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
    ("web_server_config", "wifi_config", "has_captive_portal", "expected"),
    [
        # AP only: local is implied, web_server is the captive portal.
        ({CONF_VERSION: 2}, AP_ONLY, False, True),
        # AP fallback needs an explicit local: true to be captive.
        ({CONF_VERSION: 2}, AP_FALLBACK, False, False),
        ({CONF_VERSION: 2, CONF_LOCAL: True}, AP_FALLBACK, False, True),
        # captive_portal owns the role when configured.
        ({CONF_VERSION: 2}, AP_ONLY, True, False),
        # No AP, hosted page, or version 1: never captive.
        ({CONF_VERSION: 2, CONF_LOCAL: True}, STA_ONLY, False, False),
        ({CONF_VERSION: 2, CONF_LOCAL: False}, AP_ONLY, False, False),
        ({CONF_VERSION: 1}, AP_ONLY, False, False),
    ],
)
def test_serve_captive(
    web_server_config: dict,
    wifi_config: dict | None,
    has_captive_portal: bool,
    expected: bool,
) -> None:
    assert serve_captive(web_server_config, wifi_config, has_captive_portal) is expected


def test_final_validate_ap_mode_warns_for_hosted_page(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """AP only with an explicit local: false gets a warning; AP only default does not."""
    for web_server_config, expect_warning in (
        ({CONF_VERSION: 2, CONF_LOCAL: False}, True),
        ({CONF_VERSION: 2}, False),
    ):
        caplog.clear()
        token = fv.full_config.set(
            {"web_server": web_server_config, CONF_WIFI: AP_ONLY}
        )
        try:
            with caplog.at_level(logging.WARNING):
                _final_validate_ap_mode(web_server_config)
        finally:
            fv.full_config.reset(token)
        assert ("stays blank" in caplog.text) is expect_warning
