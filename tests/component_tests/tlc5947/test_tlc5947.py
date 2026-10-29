"""Tests for the tlc5947 output channel validation."""

from collections.abc import Callable
from pathlib import Path

from esphome.config import load_config
from esphome.core import CORE


def test_last_channel_of_last_chip_is_accepted(
    component_config_path: Callable[[str], Path],
) -> None:
    CORE.config_path = component_config_path("channel_47.yaml")
    assert not load_config({}).errors


def test_channel_past_last_chip_is_rejected(
    component_config_path: Callable[[str], Path],
) -> None:
    """The hub keeps 24 duty values per chip."""
    CORE.config_path = component_config_path("channel_48.yaml")
    errors = load_config({}).errors
    assert len(errors) == 1
    assert "Channel must be less than 48" in str(errors[0])
    assert errors[0].path[-1] == "channel"
