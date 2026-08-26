"""Unit tests for doc-link surfacing in esphome.config's validation error formatting."""

from unittest.mock import patch

import voluptuous as vol

from esphome.config import (
    Config,
    _format_vol_invalid,
    _get_parent_name,
    _manifest_for_domain,
)
from esphome.loader import register_external_component_doc_url


def test_get_parent_name_empty_path_is_root_not_domain() -> None:
    """An empty path (e.g. an error on an entirely unknown top-level key) reports
    "<root>", which is never a real domain -- doc-link lookup must not be attempted."""
    assert _get_parent_name([], Config()) == ("<root>", False)


def test_manifest_for_domain_rejects_non_string_and_root() -> None:
    """Non-string and "<root>" domains have no meaningful component to link to -> None."""
    assert _manifest_for_domain(3) is None
    assert _manifest_for_domain("<root>") is None


def test_manifest_for_domain_resolves_platform_domain() -> None:
    """A "<platform>.<component>" domain (e.g. a config error inside a sensor platform)
    resolves to that platform's own manifest, not the parent domain's."""
    manifest = _manifest_for_domain("sensor.dht")
    assert manifest is not None
    assert manifest.module.__name__ == "esphome.components.dht.sensor"


def test_format_vol_invalid_appends_doc_link_when_known() -> None:
    """A component with a registered doc URL gets a doc link appended to its error message."""
    register_external_component_doc_url("wifi", "https://example.com/docs")

    config = Config()
    config.add_output_path(["wifi"], "wifi")
    err = vol.RequiredFieldInvalid("required key not provided", path=["wifi", "ssid"])

    message = _format_vol_invalid(err, config)

    assert "See https://example.com/docs/components/wifi/ for documentation." in message


def test_format_vol_invalid_no_link_when_doc_url_unknown() -> None:
    """Built-in components without a registered doc URL get no link appended (no regression)."""
    config = Config()
    config.add_output_path(["wifi"], "wifi")
    err = vol.RequiredFieldInvalid("required key not provided", path=["wifi", "ssid"])

    message = _format_vol_invalid(err, config)

    assert "See " not in message


def test_format_vol_invalid_skips_lookup_for_sub_item_error() -> None:
    """A sub-item error (path deeper than any registered output path) falls back to an
    arbitrary nested config key, not a component domain. That key must never be looked
    up as a component: it isn't one, and doing so would both spam an "Unable to import
    component" log line for every such error and, if the key happens to collide with a
    real component name (e.g. `button`, `light`, `sensor`), attach an unrelated doc link."""
    register_external_component_doc_url("button", "https://example.com/docs")

    config = Config()
    config.add_output_path(["lvgl", 0], "lvgl")
    err = vol.Invalid(
        "extra keys not allowed",
        path=["lvgl", 0, "widgets", 0, "button", "bad_key"],
    )

    with patch("esphome.config.get_component") as mock_get_component:
        message = _format_vol_invalid(err, config)

    mock_get_component.assert_not_called()
    assert "See " not in message
