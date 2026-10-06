"""Tests for the nRF52 configuration validation."""

import pytest

from esphome.components.nrf52 import _detect_bootloader
import esphome.config_validation as cv


def test_detect_bootloader_reports_a_missing_board() -> None:
    """The bootloader check runs before the schema, so it reports the missing key."""
    with pytest.raises(cv.Invalid, match="'board' is a required option"):
        _detect_bootloader({})
