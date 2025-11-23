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
