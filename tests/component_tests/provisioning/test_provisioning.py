"""Tests for the provisioning component config validation."""

from __future__ import annotations

import logging

import pytest

from esphome import config_validation as cv
from esphome.components.provisioning import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    register_source,
    report_hardcoded_credentials,
)
from esphome.const import CONF_TIMEOUT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_provisioning_requires_a_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Provisioning with no registered source is a config error.

    Sources register themselves during their own config validation; with none
    registered the window could never resolve, so validation fails.
    """
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="provisioning-capable component"):
        FINAL_VALIDATE_SCHEMA({})


def test_provisioning_accepts_a_registered_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A component that registered as a provisioning source satisfies validation."""
    set_core_config(PlatformFramework.ESP32_IDF)
    register_source("network")
    # Should not raise.
    assert FINAL_VALIDATE_SCHEMA({}) == {}


def test_provisioning_warns_on_hardcoded_credentials(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A source with credentials set in the config triggers a warning."""
    set_core_config(PlatformFramework.ESP32_IDF)
    register_source("network")
    report_hardcoded_credentials("wifi")
    with caplog.at_level(logging.WARNING):
        assert FINAL_VALIDATE_SCHEMA({}) == {}
    assert "wifi" in caplog.text
    assert "credentials" in caplog.text


def test_provisioning_no_warning_without_hardcoded_credentials(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """No credentials warning when no source reports hardcoded credentials."""
    set_core_config(PlatformFramework.ESP32_IDF)
    register_source("network")
    with caplog.at_level(logging.WARNING):
        assert FINAL_VALIDATE_SCHEMA({}) == {}
    assert "credentials" not in caplog.text


def test_provisioning_rejects_zero_timeout(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A zero timeout would leave the window open forever, so it is rejected."""
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_TIMEOUT: "0s"})


def test_provisioning_accepts_positive_timeout(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A positive timeout is accepted."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA({CONF_TIMEOUT: "5min"})
    assert config[CONF_TIMEOUT].total_milliseconds == 300000
