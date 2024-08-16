"""This helper module tracks commonly used types in the esphome python codebase."""

from typing import Union

from esphome.core import ID, EsphomeCore, Lambda

ConfigFragmentType = Union[
    str,
    int,
    float,
    None,
    dict[Union[str, int], "ConfigFragmentType"],
    list["ConfigFragmentType"],
    ID,
    Lambda,
]
ConfigType = dict[str, ConfigFragmentType]
CoreType = EsphomeCore
ConfigPathType = Union[str, int]
