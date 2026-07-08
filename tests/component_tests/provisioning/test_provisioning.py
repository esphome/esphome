"""Tests for the provisioning component config validation."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.provisioning import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    register_source,
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
