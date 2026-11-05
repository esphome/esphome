"""Tests for Tuya select options shape validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.tuya.select import CONFIG_SCHEMA, ensure_option_map
from esphome.schema_extractors import SCHEMA_EXTRACT


def _config(**kwargs) -> dict:
    return {"name": "Test Select", **kwargs}


def test_string_datapoint_accepts_list() -> None:
    config = CONFIG_SCHEMA(
        _config(string_datapoint=5, options=["auto", "manual", "eco"])
    )

    assert config["options"] == ["auto", "manual", "eco"]


def test_enum_datapoint_accepts_mapping() -> None:
    config = CONFIG_SCHEMA(_config(enum_datapoint=5, options={0: "low", 1: "high"}))

    assert config["options"] == {0: "low", 1: "high"}


def test_string_datapoint_rejects_mapping() -> None:
    with pytest.raises(cv.Invalid, match="must be a list of strings"):
        CONFIG_SCHEMA(_config(string_datapoint=5, options={0: "low", 1: "high"}))


def test_enum_datapoint_rejects_list() -> None:
    with pytest.raises(cv.Invalid, match="must be a mapping"):
        CONFIG_SCHEMA(_config(enum_datapoint=5, options=["low", "high"]))


def test_int_datapoint_rejects_list() -> None:
    with pytest.raises(cv.Invalid, match="must be a mapping"):
        CONFIG_SCHEMA(_config(int_datapoint=5, options=["low", "high"]))


def test_string_datapoint_rejects_empty_list() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_config(string_datapoint=5, options=[]))


def test_option_map_shape_is_extractable() -> None:
    # The language-schema dumper must see the numeric mapping form of
    # 'options', not just the string-list branch of the cv.Any
    data = ensure_option_map(SCHEMA_EXTRACT)

    assert isinstance(data, cv.Schema)
    assert data({0: "low", 1: "high"}) == {0: "low", 1: "high"}
