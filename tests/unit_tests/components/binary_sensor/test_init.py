"""Tests for the binary_sensor `invert` filter configuration schema."""

from __future__ import annotations

import pytest

from esphome.components import binary_sensor
import esphome.components.switch  # noqa: F401  (registers the switch.is_on condition)
import esphome.config_validation as cv
from esphome.core import Lambda

pytestmark = pytest.mark.usefixtures("setup_core")


def test_invert_filter_bare_defaults_to_true() -> None:
    """A bare `invert:`/`invert` filter entry always inverts."""
    assert binary_sensor.validate_invert_filter({}) is True


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (True, True),
        (False, False),
        ("yes", True),
        ("no", False),
        ("true", True),
        ("false", False),
    ],
)
def test_invert_filter_boolean_constant(value: bool | str, expected: bool) -> None:
    """A constant boolean (or boolean-like string) is normalized to a bool."""
    assert binary_sensor.validate_invert_filter(value) is expected


def test_invert_filter_invalid_boolean_raises() -> None:
    with pytest.raises(cv.Invalid):
        binary_sensor.validate_invert_filter("not_a_boolean")


def test_invert_filter_lambda_passthrough() -> None:
    """A `!lambda` value is left as-is for codegen to process."""
    lambda_ = Lambda("return id(some_switch).state;")
    result = binary_sensor.validate_invert_filter(lambda_)
    assert isinstance(result, Lambda)
    assert result.value == "return id(some_switch).state;"


def test_invert_filter_automation_condition() -> None:
    """A dict names an automation condition (e.g. `switch.is_on: my_switch`)."""
    result = binary_sensor.validate_invert_filter({"switch.is_on": "my_switch"})
    assert isinstance(result, dict)
    assert "switch.is_on" in result
    assert "type_id" in result


def test_invert_filter_condition_list_becomes_and() -> None:
    """A list of conditions is combined with `and`."""
    result = binary_sensor.validate_invert_filter(
        [{"switch.is_on": "a"}, {"switch.is_on": "b"}]
    )
    assert "and" in result
    assert len(result["and"]) == 2


def test_invert_filter_unknown_condition_raises() -> None:
    with pytest.raises(cv.Invalid):
        binary_sensor.validate_invert_filter({"no_such_condition": {}})


def test_invert_filter_registry_bare_string() -> None:
    """The `invert` filter can be used bare (no colon/value) in a filters list."""
    result = binary_sensor.validate_filters(["invert"])
    assert result[0]["invert"] is True


def test_invert_filter_registry_empty_value() -> None:
    """`invert:` with no value after the colon behaves like the bare form."""
    result = binary_sensor.validate_filters([{"invert": None}])
    assert result[0]["invert"] is True


def test_invert_filter_registry_explicit_false() -> None:
    result = binary_sensor.validate_filters([{"invert": False}])
    assert result[0]["invert"] is False
