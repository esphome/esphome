"""Validate the sync-word limit before firmware generation."""

import pytest

from esphome.components.sx126x import CONF_SYNC_VALUE, CONFIG_SCHEMA
import esphome.config_validation as cv


@pytest.mark.parametrize("size", [0, 1, 2, 8])
def test_supported_sync_lengths(size: int) -> None:
    value = [0x14] * size
    assert CONFIG_SCHEMA.schema[CONF_SYNC_VALUE](value) == value


@pytest.mark.parametrize("size", [9, 256])
def test_oversized_sync_word(size: int) -> None:
    with pytest.raises(cv.Invalid, match="at most 8"):
        CONFIG_SCHEMA.schema[CONF_SYNC_VALUE]([0x14] * size)
