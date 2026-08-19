"""Tests for the web_server component index page builder."""

from esphome.components.web_server import build_index_html
from esphome.const import CONF_CSS_URL, CONF_JS_URL


def test_build_index_html_has_offline_hint() -> None:
    """The hosted script tag shows a hint when the browser cannot download it."""
    html = build_index_html(
        {CONF_JS_URL: "https://oi.esphome.io/v2/www.js", CONF_CSS_URL: ""}
    )
    assert '<script src="https://oi.esphome.io/v2/www.js" onerror="' in html
    assert "local: true" in html
    assert "https://oi.esphome.io/v2/www.js. This browser needs internet" in html


def test_build_index_html_escapes_hint() -> None:
    """Quotes in the script URL cannot break out of the onerror attribute."""
    html = build_index_html({CONF_JS_URL: "http://x/a'b\"c.js", CONF_CSS_URL: ""})
    assert "onerror=\"document.body.innerText='" in html
    assert "a\\&#x27;b&quot;c.js" in html


def test_build_index_html_without_js_url() -> None:
    """No hosted script and no hint when js_url is empty."""
    html = build_index_html({CONF_JS_URL: "", CONF_CSS_URL: ""})
    assert "onerror" not in html
    assert "<script" not in html
