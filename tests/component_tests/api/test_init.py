"""Tests for the api component config validation."""

from __future__ import annotations

import pytest
from voluptuous import Invalid

from esphome.components.api import (
    CONF_ENCRYPTION,
    CONF_ON_PROVISIONING_TIMEOUT,
    CONF_PROVISIONING_TIMEOUT,
    _validate_provisioning_timeout,
)
from esphome.const import CONF_KEY
from esphome.core import TimePeriod

# A valid base64, 32-byte encryption key.
VALID_KEY = "bOFFzzvfpg5DB94DuBGLXD/hMnhpDKgP9UQyBulwWVU="


def _config(timeout_ms: int, **extra):
    config = {CONF_PROVISIONING_TIMEOUT: TimePeriod(milliseconds=timeout_ms)}
    config.update(extra)
    return config


class TestValidateProvisioningTimeout:
    """The provisioning window requires encryption enabled with no key."""

    def test_disabled_without_encryption_passes(self):
        """The default (disabled) needs no encryption."""
        config = _config(0)
        assert _validate_provisioning_timeout(config) is config

    def test_disabled_with_trigger_raises(self):
        """on_provisioning_timeout is meaningless without a timeout."""
        config = _config(0, **{CONF_ON_PROVISIONING_TIMEOUT: []})
        with pytest.raises(Invalid, match="requires 'provisioning_timeout'"):
            _validate_provisioning_timeout(config)

    def test_enabled_without_encryption_raises(self):
        config = _config(60000)
        with pytest.raises(Invalid, match="requires 'encryption:'"):
            _validate_provisioning_timeout(config)

    def test_enabled_with_yaml_key_raises(self):
        """A device born provisioned (YAML key) has no window to gate."""
        config = _config(60000, **{CONF_ENCRYPTION: {CONF_KEY: VALID_KEY}})
        with pytest.raises(Invalid, match="without a"):
            _validate_provisioning_timeout(config)

    def test_enabled_with_keyless_encryption_passes(self):
        config = _config(60000, **{CONF_ENCRYPTION: {}})
        assert _validate_provisioning_timeout(config) is config
