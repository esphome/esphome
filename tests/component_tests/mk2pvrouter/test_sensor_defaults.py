"""Tag-based sensor defaults, driven through the real CONFIG_SCHEMA."""

import pytest

from esphome.components import sensor
from esphome.components.mk2pvrouter.sensor import CONFIG_SCHEMA, tag_kind
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType


def _sensor(tag: str, **extra: object) -> ConfigType:
    return CONFIG_SCHEMA(
        {"tag": tag, "mk2pvrouter_id": "hub", "name": f"{tag} sensor", **extra}
    )


@pytest.mark.parametrize(
    ("tag", "unit", "device_class", "state_class", "decimals"),
    [
        ("P", "W", "power", "measurement", 0),
        ("P1", "W", "power", "measurement", 0),
        ("D", "W", "power", "measurement", 0),
        ("D1", "%", "", "measurement", 0),
        ("V", "V", "voltage", "measurement", 2),
        ("v1", "V", "voltage", "measurement", 2),
        ("E", "Wh", "energy", "total_increasing", 0),
        ("T1", "°C", "temperature", "measurement", 2),
        ("R", "W", "power", "measurement", 0),
        ("R1", "", "", "", 0),
        ("R10", "", "", "", 0),
    ],
)
def test_tag_defaults(
    tag: str, unit: str, device_class: str, state_class: str, decimals: int
) -> None:
    config = _sensor(tag)
    assert config[CONF_UNIT_OF_MEASUREMENT] == unit
    assert config[CONF_DEVICE_CLASS] == device_class
    assert config[CONF_STATE_CLASS] == sensor.validate_state_class(state_class)
    assert config[CONF_ACCURACY_DECIMALS] == decimals


@pytest.mark.parametrize("tag", ["S_MC", "STATUS", "X9", "Z", "T", "E1"])
def test_unknown_tag_gets_only_the_schema_defaults(tag: str) -> None:
    config = _sensor(tag)
    assert CONF_UNIT_OF_MEASUREMENT not in config
    assert CONF_DEVICE_CLASS not in config
    assert config[CONF_STATE_CLASS] == sensor.validate_state_class(
        STATE_CLASS_MEASUREMENT
    )
    assert config[CONF_ACCURACY_DECIMALS] == 0


def test_explicit_values_win_over_tag_defaults() -> None:
    config = _sensor("P", device_class=DEVICE_CLASS_EMPTY, accuracy_decimals=3)
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_EMPTY
    assert config[CONF_ACCURACY_DECIMALS] == 3
    assert config[CONF_UNIT_OF_MEASUREMENT] == "W"


@pytest.mark.parametrize(
    ("tag", "scale_centi"),
    [
        ("V", True),
        ("V1", True),
        ("v2", True),
        ("T1", True),
        ("P", False),
        ("D1", False),
        ("E", False),
        ("T", False),
        ("X9", False),
    ],
)
def test_centi_scale_follows_the_tag(tag: str, scale_centi: bool) -> None:
    kind = tag_kind(tag)
    assert (kind is not None and kind.scale_centi) is scale_centi
