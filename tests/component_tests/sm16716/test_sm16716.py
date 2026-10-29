"""Tests for the sm16716 output channel validation."""

from collections.abc import Callable
from pathlib import Path

from esphome.config import load_config
from esphome.core import CORE


def test_channel_within_num_channels_is_accepted(
    component_config_path: Callable[[str], Path],
) -> None:
    CORE.config_path = component_config_path("channel_2.yaml")
    assert not load_config({}).errors


def test_channel_past_num_channels_is_rejected(
    component_config_path: Callable[[str], Path],
) -> None:
    """write_channel indexes a buffer of num_channels entries."""
    CORE.config_path = component_config_path("channel_3.yaml")
    errors = load_config({}).errors
    assert len(errors) == 1
    assert "Channel must be less than 3" in str(errors[0])
    assert errors[0].path[-1] == "channel"
