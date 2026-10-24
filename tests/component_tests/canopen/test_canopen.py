"""Validate CANopen addressing and numeric type configuration."""

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components.canopen import (
    CONFIG_SCHEMA as HUB_SCHEMA,
    NMT_SCHEMA,
    SDO_SCHEMA,
    SDO_WRITE_SCHEMA,
    validate_single_hub_per_bus,
)
from esphome.components.canopen.sensor import CONFIG_SCHEMA
from esphome.core import Lambda


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


@pytest.mark.parametrize("timeout", ["0ms", "-1ms", "60001ms"])
def test_invalid_response_timeout(timeout: str) -> None:
    with pytest.raises(cv.Invalid):
        HUB_SCHEMA({"response_timeout": timeout})


@pytest.mark.parametrize("timeout", ["1ms", "500ms", "60s"])
def test_valid_response_timeout(timeout: str) -> None:
    assert (
        HUB_SCHEMA({"response_timeout": timeout})["response_timeout"].total_milliseconds
        > 0
    )


@pytest.mark.parametrize("size", [1, 2, 4])
def test_write_value_width(size: int) -> None:
    config = {
        "node_id": 32,
        "index": 0x2000,
        "size": size,
        "value": (1 << (8 * size)) - 1,
    }
    assert SDO_WRITE_SCHEMA(config)["value"] == config["value"]
    config["value"] += 1
    with pytest.raises(cv.Invalid):
        SDO_WRITE_SCHEMA(config)


def test_write_lambda_is_validated_at_runtime() -> None:
    config = SDO_WRITE_SCHEMA(
        {"node_id": 32, "index": 0x2000, "size": 1, "value": Lambda("return 300;")}
    )
    assert isinstance(config["value"], Lambda)


@pytest.mark.parametrize("node", [0, 1, 127])
@pytest.mark.parametrize(
    "command", ["START", "STOP", "PRE_OPERATIONAL", "RESET_NODE", "RESET_COMMUNICATION"]
)
def test_nmt_commands(node: int, command: str) -> None:
    assert NMT_SCHEMA({"node_id": node, "command": command})["node_id"] == node


@pytest.mark.parametrize(
    "node,command", [(-1, "START"), (128, "START"), (32, "INVALID")]
)
def test_invalid_nmt(node: int, command: str) -> None:
    with pytest.raises(cv.Invalid):
        NMT_SCHEMA({"node_id": node, "command": command})
