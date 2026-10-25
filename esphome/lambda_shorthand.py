"""Type classification for the `entity_state:`/`argument:` lambda shorthand, shared
by `esphome.config_validation` and `esphome.cpp_generator`."""

from __future__ import annotations

from functools import cache
from typing import Any

from esphome.enum import StrEnum


class ArgKind(StrEnum):
    """Coarse category for a C++ value's type. Used only to catch a shorthand
    reference that's guaranteed not to compile (e.g. a std::string state fed into a
    numeric field) -- not to police every stylistic mismatch.
    """

    NUMERIC = "numeric"
    BOOLEAN = "boolean"
    STRING = "string"


def kinds_compatible(a: ArgKind, b: ArgKind) -> bool:
    """Bool and numeric both implicitly convert to each other in C++; a string
    converts to neither."""
    if a == b:
        return True
    numeric_ish = {ArgKind.NUMERIC, ArgKind.BOOLEAN}
    return a in numeric_ish and b in numeric_ish


# C++ type names (as a MockObjClass renders via str()) recognized well enough to
# sanity-check a shorthand reference against the field's expected type. Anything else
# (enums, custom classes, ...) is left unchecked, so the shorthand stays lenient rather
# than risking a false-positive rejection.
_CPP_NUMERIC_TYPE_NAMES = {
    "float",
    "double",
    "int",
    "int8_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "size_t",
}
_CPP_BOOLEAN_TYPE_NAMES = {"bool"}
_CPP_STRING_TYPE_NAMES = {"std::string", "std::string &", "const char *"}


def arg_kind(type_: Any) -> ArgKind | None:
    """The `ArgKind` of a Python builtin type or a C++ type (anything that renders
    its name via `str()`, e.g. a `MockObjClass`), or `None` if not recognized well
    enough to classify.
    """
    # Python builtin type objects turn up directly in some `args`/return-type values
    # (e.g. sensor's `on_value` uses `[(float, "x")]`); compare by identity, never
    # `==`/`in`, since MockObj overloads `==` to build a C++ expression rather than
    # compare equality.
    if type_ is bool:
        return ArgKind.BOOLEAN
    if type_ is float or type_ is int:
        return ArgKind.NUMERIC
    if type_ is str:
        return ArgKind.STRING
    type_str = str(type_)
    if type_str in _CPP_NUMERIC_TYPE_NAMES:
        return ArgKind.NUMERIC
    if type_str in _CPP_BOOLEAN_TYPE_NAMES:
        return ArgKind.BOOLEAN
    if type_str in _CPP_STRING_TYPE_NAMES:
        return ArgKind.STRING
    return None


@cache
def state_bearing_types() -> tuple[tuple[Any, ArgKind], ...]:
    """Declared-id types with a public `.state` C++ field, paired with the kind of
    that field. Not a dict because `MockObjClass` isn't hashable. The imports are
    local because component modules import `esphome.config_validation`, which
    imports this module.

    `light.LightState` is left out: it has no usable `.state` field.
    """
    from esphome.components.binary_sensor import BinarySensor
    from esphome.components.fan import Fan
    from esphome.components.number import Number
    from esphome.components.sensor import Sensor
    from esphome.components.switch import Switch
    from esphome.components.text import Text
    from esphome.components.text_sensor import TextSensor

    return (
        (Sensor, ArgKind.NUMERIC),
        (Number, ArgKind.NUMERIC),
        (BinarySensor, ArgKind.BOOLEAN),
        (Switch, ArgKind.BOOLEAN),
        (Fan, ArgKind.BOOLEAN),
        (Text, ArgKind.STRING),
        (TextSensor, ArgKind.STRING),
    )
