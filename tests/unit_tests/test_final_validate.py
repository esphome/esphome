"""Tests for esphome.final_validate helpers."""

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.config import Config
from esphome.const import CONF_ID
from esphome.core import ID


def _config() -> tuple[Config, ID, dict]:
    hub_id = ID("my_hub", is_declaration=True)
    hub = {CONF_ID: hub_id, "mode": "fast"}
    config = Config()
    config["other"] = {"x": 1}
    config["my_comp"] = [{"y": 2}, hub]
    config.declare_ids.append((hub_id, ["my_comp", 1, CONF_ID]))
    return config, hub_id, hub


def test_returns_the_declaring_block_from_the_full_config() -> None:
    config, hub_id, hub = _config()
    token = fv.full_config.set(config)
    try:
        assert fv.get_declaring_config(hub_id) is hub
    finally:
        fv.full_config.reset(token)


def test_uses_the_given_config() -> None:
    config, hub_id, hub = _config()
    assert fv.get_declaring_config(ID("my_hub"), config) is hub


def test_unknown_id_raises_key_error() -> None:
    config, _, _ = _config()
    with pytest.raises(KeyError):
        fv.get_declaring_config(ID("missing"), config)


def test_non_dict_parent_raises_type_error() -> None:
    config, _, _ = _config()
    list_id = ID("in_list", is_declaration=True)
    config["listed"] = [list_id]
    config.declare_ids.append((list_id, ["listed", 0]))
    with pytest.raises(TypeError):
        fv.get_declaring_config(ID("in_list"), config)


@pytest.mark.parametrize("pre_built", [False, True])
def test_id_declaration_match_schema_validates_declaration(pre_built: bool) -> None:
    config, hub_id, _ = _config()
    schema = {cv.Required("mode"): cv.string}
    if pre_built:
        schema = cv.Schema(schema, extra=cv.ALLOW_EXTRA)
    validator = fv.id_declaration_match_schema(schema)
    token = fv.full_config.set(config)
    try:
        result = validator(ID("my_hub"))
    finally:
        fv.full_config.reset(token)

    assert result["mode"] == "fast"


def test_id_declaration_match_schema_rejects_mismatch() -> None:
    config, _, _ = _config()
    validator = fv.id_declaration_match_schema({cv.Required("mode"): cv.one_of("slow")})
    token = fv.full_config.set(config)
    try:
        with pytest.raises(cv.Invalid):
            validator(ID("my_hub"))
    finally:
        fv.full_config.reset(token)
