"""speed_count must fit the uint16_t FanTraits stores it in."""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.hbridge.fan import CONFIG_SCHEMA as HBRIDGE_SCHEMA
from esphome.components.speed.fan import CONFIG_SCHEMA as SPEED_SCHEMA
from esphome.components.template.fan import CONFIG_SCHEMA as TEMPLATE_SCHEMA
from esphome.const import CONF_SPEED_COUNT

SCHEMAS: dict[str, tuple[Any, dict[str, Any]]] = {
    "speed": (SPEED_SCHEMA, {"name": "Fan", "output": "fan_out"}),
    "hbridge": (HBRIDGE_SCHEMA, {"name": "Fan", "pin_a": "out_a", "pin_b": "out_b"}),
    "template": (TEMPLATE_SCHEMA, {"name": "Fan"}),
}


@pytest.mark.parametrize("platform", SCHEMAS)
@pytest.mark.parametrize("speed_count", [1, 65535])
def test_speed_count_limits_accepted(platform: str, speed_count: int) -> None:
    schema, base = SCHEMAS[platform]
    config = schema({**base, CONF_SPEED_COUNT: speed_count})
    assert config[CONF_SPEED_COUNT] == speed_count


@pytest.mark.parametrize("platform", SCHEMAS)
@pytest.mark.parametrize("speed_count", [0, 65536])
def test_speed_count_out_of_range_rejected(platform: str, speed_count: int) -> None:
    schema, base = SCHEMAS[platform]
    with pytest.raises(cv.Invalid, match="speed_count|at least|at most"):
        schema({**base, CONF_SPEED_COUNT: speed_count})


def test_template_fan_without_speed_count() -> None:
    schema, base = SCHEMAS["template"]
    assert CONF_SPEED_COUNT not in schema(base)
