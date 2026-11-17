"""Tests for the fingerprint_grow component schema."""

import pytest

from esphome.components.fingerprint_grow import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.yaml_util import Sensitive
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("field", ["password", "new_password"])
def test_password_is_sensitive(
    field: str, set_core_config: SetCoreConfigCallable
) -> None:
    """Numeric sensor passwords are tagged for redaction and keep their value."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA({field: 305419896})
    assert isinstance(config[field], Sensitive)
    assert config[field] == 305419896
