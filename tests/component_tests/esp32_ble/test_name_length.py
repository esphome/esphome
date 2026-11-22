"""Tests for the esp32_ble name length, which is limited in UTF-8 bytes."""

from collections.abc import Callable
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import esp32_ble
from esphome.const import CONF_ESPHOME, CONF_NAME, CONF_NAME_ADD_MAC_SUFFIX


@pytest.mark.parametrize(
    ("name", "valid"),
    [
        ("a" * 20, True),
        ("a" * 21, False),
        ("é" * 10, True),  # 20 bytes
        ("é" * 11, False),  # 11 characters, 22 bytes
    ],
)
def test_name_schema_counts_bytes(name: str, valid: bool) -> None:
    schema = next(
        v for v in esp32_ble.CONFIG_SCHEMA.validators if isinstance(v, cv.Schema)
    )
    validator = schema.schema[cv.Optional(CONF_NAME)]
    if valid:
        assert validator(name) == name
    else:
        with pytest.raises(cv.Invalid, match="bytes"):
            validator(name)


@pytest.mark.parametrize(
    ("name", "add_suffix", "valid"),
    [
        ("a" * 13, True, True),
        ("a" * 14, True, False),
        ("é" * 7, True, False),  # 7 characters, 14 bytes
        ("é" * 7, False, True),
    ],
)
def test_name_with_mac_suffix_counts_bytes(
    set_component_config: Callable[[str, Any], None],
    name: str,
    add_suffix: bool,
    valid: bool,
) -> None:
    set_component_config(CONF_ESPHOME, {CONF_NAME_ADD_MAC_SUFFIX: add_suffix})
    config = {CONF_NAME: name}
    if valid:
        esp32_ble.validate_name_with_mac_suffix(config)
    else:
        with pytest.raises(cv.Invalid, match="name_add_mac_suffix"):
            esp32_ble.validate_name_with_mac_suffix(config)
