"""Tests for the provisioning component config validation."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv

# Importing the api component registers its provisioning-source detector at import
# time (module-level register_provisioning_source call), which these tests rely on.
from esphome.components.api import CONF_ENCRYPTION
from esphome.components.provisioning import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import CONF_ESPHOME, CONF_TIMEOUT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_provisioning_requires_a_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Provisioning with no provisioning-capable component is a config error."""
    set_core_config(PlatformFramework.ESP32_IDF, full_config={CONF_ESPHOME: {}})
    with pytest.raises(cv.Invalid, match="provisioning-capable component"):
        FINAL_VALIDATE_SCHEMA({})


def test_provisioning_accepts_api_encryption_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """API with encryption enabled is a valid provisioning source."""
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config={"api": {CONF_ENCRYPTION: {}}}
    )
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
