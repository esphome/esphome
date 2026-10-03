"""Shared classification for the `entity_state:`/`argument:` lambda shorthand.

Used by both `esphome.config_validation` (to build the shorthand's expanded lambda
and to know which declared-id types `entity_state:` may point at) and
`esphome.cpp_generator` (to sanity-check a shorthand lambda's resolved type against
the field it feeds, at codegen time when the field's actual C++ return type is
known). Kept as its own leaf module -- importing nothing from either -- so the two
don't have to reach into each other's private names for one shared concept.
"""

from __future__ import annotations

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


_STATE_BEARING_TYPES = None


def state_bearing_types() -> list:
    """Declared-id types with a public, directly-usable `.state` C++ field, and the
    kind of that field, as a list of `(type, ArgKind)` pairs -- `MockObjClass` isn't
    hashable, so this can't be a dict keyed by type. Lazily imported: importing
    component modules at this module's top level would be circular, since every
    component module imports `esphome.config_validation`, which imports this module.

    `light.LightState` is deliberately not included here: unlike the others, it has
    no directly-usable `.state` field (the boolean "on" state lives behind
    `remote_values.is_on()`), so `entity_state:` on a light id would validate cleanly
    and then fail to compile.
    """
    global _STATE_BEARING_TYPES  # noqa: PLW0603
    if _STATE_BEARING_TYPES is None:
        from esphome.components.binary_sensor import BinarySensor
        from esphome.components.fan import Fan
        from esphome.components.number import Number
        from esphome.components.sensor import Sensor
        from esphome.components.switch import Switch
        from esphome.components.text_sensor import TextSensor

        _STATE_BEARING_TYPES = [
            (Sensor, ArgKind.NUMERIC),
            (Number, ArgKind.NUMERIC),
            (BinarySensor, ArgKind.BOOLEAN),
            (Switch, ArgKind.BOOLEAN),
            (Fan, ArgKind.BOOLEAN),
            (TextSensor, ArgKind.STRING),
        ]
    return _STATE_BEARING_TYPES
