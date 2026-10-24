import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components.cia402 import CONFIG_SCHEMA, MOVE_SCHEMA, validate_node_owner

BASE = {
    "node_id": 32,
    "profile_velocity": 1000,
    "acceleration": 2000,
    "deceleration": 2000,
}


@pytest.mark.parametrize(
    "key,value",
    [
        ("node_id", 0),
        ("node_id", 128),
        ("profile_velocity", 0),
        ("acceleration", -1),
        ("deceleration", 0),
        ("heartbeat_timeout", "0ms"),
        ("operation_timeout", "0ms"),
        ("quick_stop_deceleration", 0),
        ("homing_fast_velocity", -1),
        ("homing_slow_velocity", 0),
        ("homing_acceleration", 0),
    ],
)
def test_reject_invalid_drive(key: str, value: object) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**BASE, key: value})


def test_signed_position() -> None:
    assert MOVE_SCHEMA({"position": -2147483648})["position"] == -2147483648
    with pytest.raises(cv.Invalid):
        MOVE_SCHEMA({"position": 2147483648})


def test_duplicate_owner() -> None:
    node = {"canopen_id": "bus", "node_id": 32}
    token = fv.full_config.set({"cia402": [node, node]})
    try:
        with pytest.raises(cv.Invalid, match="Only one"):
            validate_node_owner(node)
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize(
    "limits",
    [
        {"min_position": 0},
        {"max_position": 100},
        {"min_position": 100, "max_position": 0},
        {"min_position": -2147483649, "max_position": 0},
    ],
)
def test_invalid_limits(limits: dict) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**BASE, **limits})


def test_signed_limits() -> None:
    config = CONFIG_SCHEMA({**BASE, "min_position": -100, "max_position": 100})
    assert config["min_position"] == -100
    assert config["max_position"] == 100
