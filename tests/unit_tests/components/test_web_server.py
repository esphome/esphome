"""Tests for the web_server component index page builder."""

from esphome.components.web_server import build_index_html
from esphome.const import CONF_CSS_URL, CONF_JS_URL


def test_build_index_html_has_offline_hint() -> None:
    """The hosted script tag shows a hint when the browser cannot download it."""
    html = build_index_html(
        {CONF_JS_URL: "https://oi.esphome.io/v2/www.js", CONF_CSS_URL: ""}
    )
    assert '<script src="https://oi.esphome.io/v2/www.js" onerror="' in html
    assert "document.body.innerText='Could not download the web interface." in html
    assert "local: true" in html


def test_build_index_html_hint_disabled() -> None:
    """The plain script tag is kept when the hint is not wanted (captive_portal)."""
    html = build_index_html(
        {CONF_JS_URL: "https://oi.esphome.io/v2/www.js", CONF_CSS_URL: ""},
        offline_hint=False,
    )
    assert '<script src="https://oi.esphome.io/v2/www.js"></script>' in html
    assert "onerror" not in html


def test_build_index_html_without_js_url() -> None:
    """No hosted script and no hint when js_url is empty."""
    html = build_index_html({CONF_JS_URL: "", CONF_CSS_URL: ""})
    assert "onerror" not in html
    assert "<script" not in html
