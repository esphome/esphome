"""Unit tests for doc-link surfacing in esphome.config's validation error formatting."""

import voluptuous as vol

from esphome.config import Config, _format_vol_invalid
from esphome.loader import register_external_component_doc_url


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
