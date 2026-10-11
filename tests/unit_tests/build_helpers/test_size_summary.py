"""Tests for the shared PlatformIO-format size bar."""

from __future__ import annotations

import pytest

from esphome.build_helpers.size_summary import format_bar, print_size_line


def test_format_bar_zero_total() -> None:
    """A zero total must not divide by zero."""
    assert format_bar(0, 0) == "[          ]   0.0% (used 0 bytes from 0 bytes)"


def test_print_size_line_label_padding(capsys: pytest.CaptureFixture[str]) -> None:
    """The label column is exactly what ci_memory_impact_extract.py greps."""
    print_size_line("RAM", 47932, 180736)
    print_size_line("Flash", 888511, 1835008)
    out = capsys.readouterr().out.splitlines()
    assert out[0].startswith("RAM:   [")
    assert out[1].startswith("Flash: [")
    assert "26.5% (used 47932 bytes from 180736 bytes)" in out[0]
