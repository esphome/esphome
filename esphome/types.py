"""This helper module tracks commonly used types in the esphome python codebase."""

import abc
from collections.abc import Sequence
from typing import Any, TypedDict

from esphome.core import ID, EsphomeCore, Lambda, TimePeriod

ConfigFragmentType = (
    str
    | int
    | float
    | None
    | dict[str | int, "ConfigFragmentType"]
    | list["ConfigFragmentType"]
    | ID
    | Lambda
)

ConfigType = dict[str, ConfigFragmentType]
CoreType = EsphomeCore
ConfigPathType = str | int


class Expression(abc.ABC):
    __slots__ = ()

    @abc.abstractmethod
    def __str__(self):
        """
        Convert expression into C++ code
        """


SafeExpType = (
    Expression
    | bool
    | str
    | int
    | float
    | TimePeriod
    | type[bool]
    | type[int]
    | type[float]
    | Sequence[Any]
)

TemplateArgsType = list[tuple[SafeExpType, str]]


class EntityMetadata(TypedDict):
    """Metadata stored for each entity to help with duplicate detection."""

    name: str
    device_id: str
    platform: str
    entity_id: str
    component: str


class Extend:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"!extend {self.value}"

    def __repr__(self):
        return f"Extend({self.value})"

    def __eq__(self, b):
        """
        Check if two Extend objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Extend) and self.value == b.value


class Remove:
    def __init__(self, value=None):
        self.value = value

    def __str__(self):
        return f"!remove {self.value}"

    def __repr__(self):
        return f"Remove({self.value})"

    def __eq__(self, b):
        """
        Check if two Remove objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Remove) and self.value == b.value
