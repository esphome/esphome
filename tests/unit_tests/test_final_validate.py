"""Tests for esphome.final_validate helpers."""

import pytest

from esphome import final_validate as fv
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
