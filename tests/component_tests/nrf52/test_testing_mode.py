"""Tests for the flash size faked in testing mode."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_OVERLAY
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "has_flash0"),
    [
        ("nrf52840.yaml", True),
        ("nrf54l15dk.yaml", False),
        ("nrf54lm20dk.yaml", False),
    ],
)
def test_testing_mode_flash_overlay(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    has_flash0: bool,
) -> None:
    """The nRF54L has no flash0 node, so the overlay must not refer to it there."""
    CORE.testing_mode = True
    generate_main(component_config_path(config))
    assert ("&flash0" in zephyr_data()[KEY_OVERLAY][""]) is has_flash0
