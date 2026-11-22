"""The stack buffer for a format text must hold any output of the format."""

from __future__ import annotations

import pytest

from esphome.components.lvgl.lv_validation import format_buffer_size


@pytest.mark.parametrize(
    ("fmt", "size"),
    [
        ("plain", 6),
        ("%.1f°C", 46),
        ("%.0f%%", 42),
        ("%f", 48),
        ("%+.1f", 43),
        ("\U000f015f %.0f° | %.0f\U000f03f0 | %.0fhPa", 141),
        ("%d", 12),
        ("%-5d", 12),
        ("%hhd", 12),
        ("%d bells, %d things", 38),
        ("Row %lu", 25),
        ("%u items", 17),
        ("%08x", 9),
        ("%#x", 11),
        ("%#o", 14),
        ("%.12d", 14),
        ("%c", 2),
        ("%5c", 6),
        ("%%", 2),
        ("%e", 15),
        ("%E", 15),
        ("%.3g", 12),
        ("%G", 15),
        ("%50d", 51),
    ],
)
def test_bounded_formats(fmt: str, size: int) -> None:
    assert format_buffer_size(fmt) == size


@pytest.mark.parametrize(
    "fmt",
    [
        "%s",
        "\U000f050f %s °C",
        "%s, %02d.%02d.%04d",
        "%p",
        "%*d",
        "%.*f",
        "%Lf",
        "%a",
        "%n",
        "% °F",
        "%5%",
        "%-%",
        "100%",
        "%300d",
    ],
)
def test_unbounded_formats(fmt: str) -> None:
    assert format_buffer_size(fmt) is None
