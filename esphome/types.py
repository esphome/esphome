"""This helper module tracks commonly used types in the esphome python codebase."""

from typing import TypedDict

from esphome.core import ID, EsphomeCore, Lambda

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


class EntityMetadata(TypedDict):
    """Metadata stored for each entity to help with duplicate detection."""

    name: str
    device_id: str
    platform: str
    entity_id: str
    component: str
