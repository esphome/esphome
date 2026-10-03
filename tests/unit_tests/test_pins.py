"""Unit tests for esphome/pins.py."""

import pytest

from esphome import config_validation as cv, pins
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS
from esphome.core import ID
from esphome.schema_extractors import SCHEMA_EXTRACT

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


def test_use_id_or_address_marks_address_match_as_manual() -> None:
    """The resolved id must survive strip_default_ids(), unlike a plain
    omitted-id auto-pick, since the user explicitly gave selection criteria."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator({CONF_ADDRESS: 0x21})

    assert result.is_manual is True


def test_use_id_or_address_schema_extract_passthrough() -> None:
    """Called with the SCHEMA_EXTRACT sentinel (as build_language_schema.py
    does), the wrapper must fall through to cv.use_id's own handling and
    return the hub type, not try to treat it as an address mapping."""
    validator = pins.use_id_or_address(SomeHub)

    assert validator(SCHEMA_EXTRACT) is SomeHub


def test_use_id_or_address_registers_as_use_id_schema(monkeypatch) -> None:
    """The wrapper must be discoverable by build_language_schema.py as a
    `use_id` schema (like a bare cv.use_id), or the generated language schema
    silently drops hub-id completion for every pin schema using it."""
    from esphome import schema_extractors

    monkeypatch.setattr(schema_extractors, "EnableSchemaExtraction", True)
    schema_extractors.hidden_schemas.clear()

    validator = pins.use_id_or_address(SomeHub)

    assert schema_extractors.hidden_schemas.get(repr(validator)) == "use_id"
