"""Unit tests for esphome/pins.py."""

import pytest

from esphome import config_validation as cv, pins
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS
from esphome.core import ID

SomeHub = cg.esphome_ns.class_("SomeHub")


def test_use_id_or_address_with_explicit_id() -> None:
    """An explicit id string still resolves exactly like cv.use_id."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator("my_hub")

    assert isinstance(result, ID)
    assert result.id == "my_hub"
    assert result.type is SomeHub
    assert result.is_declaration is False
    assert result.match_config is None


def test_use_id_or_address_with_omitted_id() -> None:
    """Omitting the id (None) still falls back to the single-instance auto-pick."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator(None)

    assert isinstance(result, ID)
    assert result.id is None
    assert result.type is SomeHub
    assert result.match_config is None


def test_use_id_or_address_with_address_mapping() -> None:
    """A {address: ...} mapping produces an unnamed ID with match_config set."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator({CONF_ADDRESS: 0x21})

    assert isinstance(result, ID)
    assert result.id is None
    assert result.type is SomeHub
    assert result.is_declaration is False
    assert result.match_config == {CONF_ADDRESS: 0x21}


def test_use_id_or_address_with_string_address() -> None:
    """A hex string address is normalized the same way cv.i2c_address does."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator({CONF_ADDRESS: "0x21"})

    assert result.match_config == {CONF_ADDRESS: 0x21}


def test_use_id_or_address_mapping_requires_address_key() -> None:
    """A mapping without the address key is rejected, not silently ignored."""
    validator = pins.use_id_or_address(SomeHub)

    with pytest.raises(cv.Invalid):
        validator({})


def test_use_id_or_address_mapping_rejects_invalid_address() -> None:
    """An out-of-range address is still validated by cv.i2c_address."""
    validator = pins.use_id_or_address(SomeHub)

    with pytest.raises(cv.Invalid):
        validator({CONF_ADDRESS: 0x1FF})


def test_use_id_or_address_custom_address_key() -> None:
    """A custom address_key is honored both in the mapping and match_config."""
    validator = pins.use_id_or_address(SomeHub, address_key="i2c_address")
    result = validator({"i2c_address": 0x10})

    assert result.match_config == {"i2c_address": 0x10}
