"""Tests for the sm16716 output channel validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import load_config
from esphome.core import CORE


@pytest.mark.parametrize(
    ("file_name", "expected_error"),
    [
        ("channel_2.yaml", None),
        (
            "channel_3.yaml",
            "Channel must be less than 3, the number of channels of 'sm16716_hub'",
        ),
    ],
)
def test_channel_range(
    component_config_path: Callable[[str], Path],
    file_name: str,
    expected_error: str | None,
) -> None:
    """A channel past num_channels would write outside the hub's duty values."""
    CORE.config_path = component_config_path(file_name)
    errors = load_config({}).errors
    if expected_error is None:
        assert not errors
        return
    assert len(errors) == 1
    assert expected_error in str(errors[0])
    assert errors[0].path[-1] == "channel"
