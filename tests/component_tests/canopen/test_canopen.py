"""Validate CANopen addressing and numeric type configuration."""

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components.canopen import SDO_SCHEMA, validate_single_hub_per_bus
from esphome.components.canopen.sensor import CONFIG_SCHEMA


@pytest.mark.parametrize("node", [0, 128, -1])
def test_invalid_node(node: int) -> None:
    with pytest.raises(cv.Invalid):
        SDO_SCHEMA({"node_id": node, "index": 0x6041})


@pytest.mark.parametrize("index,subindex", [(65536, 0), (-1, 0), (0x6041, 256)])
def test_invalid_object(index: int, subindex: int) -> None:
    with pytest.raises(cv.Invalid):
        SDO_SCHEMA({"node_id": 32, "index": index, "subindex": subindex})


@pytest.mark.parametrize("node", [1, 32, 127])
def test_valid_node(node: int) -> None:
    config = SDO_SCHEMA({"node_id": node, "index": 0x6041})
    assert config["subindex"] == 0


def test_requires_numeric_type() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"name": "Position", "node_id": 32, "index": 0x6064})


def test_duplicate_hubs_on_one_bus_rejected() -> None:
    config = {"canbus_id": "bus_a"}
    token = fv.full_config.set({"canopen": [config, config]})
    try:
        with pytest.raises(cv.Invalid, match="Only one"):
            validate_single_hub_per_bus(config)
    finally:
        fv.full_config.reset(token)


def test_hubs_on_different_buses_allowed() -> None:
    config = {"canbus_id": "bus_a"}
    token = fv.full_config.set({"canopen": [config, {"canbus_id": "bus_b"}]})
    try:
        assert validate_single_hub_per_bus(config) == config
    finally:
        fv.full_config.reset(token)
