"""Tests for the shared PlatformIO-format size bar."""

from __future__ import annotations

from esphome.build_helpers.size_summary import format_bar


def test_format_bar_zero_total() -> None:
    """A zero total must not divide by zero."""
    assert format_bar(0, 0) == "[          ]   0.0% (used 0 bytes from 0 bytes)"
