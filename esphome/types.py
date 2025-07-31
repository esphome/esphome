"""This helper module tracks commonly used types in the esphome python codebase."""

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
