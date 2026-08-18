"""Tests for the voice_assistant runtime model loading configuration."""

import pytest

from esphome.components import voice_assistant as va
from esphome.components.http_request import CONF_HTTP_REQUEST_ID
import esphome.config_validation as cv


def test_auto_load_without_http_request_id() -> None:
    """Devices that do not download models pull in nothing extra."""
    assert va.AUTO_LOAD({}) == ["audio", "ring_buffer", "socket"]


def test_auto_load_with_http_request_id() -> None:
    """Downloading models needs sha256 to verify them and json to parse manifests."""
    auto_load = va.AUTO_LOAD({CONF_HTTP_REQUEST_ID: "http_request_component"})
    assert "sha256" in auto_load
    assert "json" in auto_load


def test_auto_load_with_no_config() -> None:
    """AUTO_LOAD is also called with an empty config while the schema is being built."""
    assert va.AUTO_LOAD(None) == ["audio", "ring_buffer", "socket"]


def test_runtime_model_validate_requires_micro_wake_word() -> None:
    """http_request_id is only useful alongside micro_wake_word, which runs the models."""
    with pytest.raises(cv.Invalid, match=va.CONF_MICRO_WAKE_WORD):
        va._runtime_model_validate({CONF_HTTP_REQUEST_ID: "http_request_component"})


def test_runtime_model_validate_accepts_both() -> None:
    config = {
        CONF_HTTP_REQUEST_ID: "http_request_component",
        va.CONF_MICRO_WAKE_WORD: "mww_component",
    }
    assert va._runtime_model_validate(config) is config


def test_runtime_model_validate_accepts_neither() -> None:
    """micro_wake_word alone stays valid; it just cannot download new models."""
    config = {va.CONF_MICRO_WAKE_WORD: "mww_component"}
    assert va._runtime_model_validate(config) is config
