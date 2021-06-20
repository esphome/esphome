"""This helper module tracks commonly used types in the esphome python codebase."""
from typing import Dict, Union, List

from esphome.core import ID, Lambda, EsphomeCore

ConfigFragmentType = Union[
    str,
    int,
    float,
    None,
    Dict[Union[str, int], "ConfigFragmentType"],
    List["ConfigFragmentType"],
    ID,
    Lambda,
]
ConfigType = Dict[str, ConfigFragmentType]
CoreType = EsphomeCore
ConfigPathType = Union[str, int]
