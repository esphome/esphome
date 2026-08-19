"""Tests for the web_server component index page builder and validation."""

import logging

import pytest

from esphome.components.web_server import (
    _final_validate_ap_only_hosted_ui,
    build_index_html,
)
from esphome.const import (
    CONF_AP,
    CONF_CSS_URL,
    CONF_JS_URL,
    CONF_LOCAL,
    CONF_NETWORKS,
    CONF_SSID,
    CONF_WIFI,
)
import esphome.final_validate as fv

JS_URL = "https://oi.esphome.io/v2/www.js"


def test_build_index_html_has_offline_hint() -> None:
    """A hidden hint is shown after a timeout unless www.js registered esp-app."""
    html = build_index_html({CONF_JS_URL: JS_URL, CONF_CSS_URL: ""})
    assert "<style>esp-app:defined+p{display:none}</style>" in html
    assert "<esp-app></esp-app><p hidden>The web interface is not loading." in html
    assert "local: true" in html
    assert "<script>setTimeout(function(){document.querySelector('p').hidden=0}" in html
    # The hint script must come before the hosted script, which may never finish loading
    assert html.index("setTimeout") < html.index(f'<script src="{JS_URL}">')


def test_build_index_html_hint_disabled() -> None:
    """Plain page when the hint is not wanted (captive_portal)."""
    html = build_index_html({CONF_JS_URL: JS_URL, CONF_CSS_URL: ""}, offline_hint=False)
    assert f'<esp-app></esp-app><script src="{JS_URL}"></script>' in html
    assert "<p" not in html
    assert "<style>" not in html


def test_build_index_html_without_js_url() -> None:
    """No hosted script and no hint when js_url is empty."""
    html = build_index_html({CONF_JS_URL: "", CONF_CSS_URL: ""})
    assert "<script" not in html
    assert "<p" not in html


@pytest.mark.parametrize(
    ("web_server_config", "full_config", "expect_warning"),
    [
        # AP only with a hosted page: warn.
        ({CONF_JS_URL: JS_URL}, {CONF_WIFI: {CONF_AP: {}}}, True),
        # local: true serves the page from the device.
        ({CONF_JS_URL: JS_URL, CONF_LOCAL: True}, {CONF_WIFI: {CONF_AP: {}}}, False),
        # No hosted script at all.
        ({CONF_JS_URL: ""}, {CONF_WIFI: {CONF_AP: {}}}, False),
        # captive_portal serves its own local page.
        (
            {CONF_JS_URL: JS_URL},
            {CONF_WIFI: {CONF_AP: {}}, "captive_portal": {}},
            False,
        ),
        # STA with AP fallback: the AP is rarely used, stay quiet.
        (
            {CONF_JS_URL: JS_URL},
            {CONF_WIFI: {CONF_AP: {}, CONF_NETWORKS: [{CONF_SSID: "x"}]}},
            False,
        ),
        # No AP, or no wifi at all (ethernet).
        (
            {CONF_JS_URL: JS_URL},
            {CONF_WIFI: {CONF_NETWORKS: [{CONF_SSID: "x"}]}},
            False,
        ),
        ({CONF_JS_URL: JS_URL}, {}, False),
    ],
)
def test_final_validate_ap_only_hosted_ui_warning(
    web_server_config: dict,
    full_config: dict,
    expect_warning: bool,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """AP only configs with a hosted page get a hint to use local: true."""
    token = fv.full_config.set({"web_server": web_server_config, **full_config})
    try:
        with caplog.at_level(logging.WARNING):
            _final_validate_ap_only_hosted_ui(web_server_config)
    finally:
        fv.full_config.reset(token)
    assert ("AP only" in caplog.text) is expect_warning
