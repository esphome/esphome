"""The stack buffer for a format text must hold any output of the format."""

from __future__ import annotations

import pytest

from esphome.components.lvgl.lv_validation import format_buffer_size


@pytest.mark.parametrize(
    ("fmt", "size"),
    [
        ("plain", 6),
        ("%.1f", 43),
        ("%.1f°C", 46),
        ("%.0f", 41),
        ("%.0f%%", 42),
        ("%f", 48),
        ("%5.2f", 44),
        ("OTA progress %0.1f%%", 57),
        ("\U000f058e %.1f \U000f03f0", 53),
        ("\U000f013b %.0f µg/m³", 54),
        ("\U000f015f %.0f° | %.0f\U000f03f0 | %.0fhPa", 141),
        ("%d", 12),
        ("%02d", 12),
        ("%3.0d", 12),
        ("Roll %+d", 17),
        ("%d°C", 15),
        ("%d bells, %d things", 38),
        ("Row %lu", 25),
        ("%ld", 21),
        ("%u items", 17),
        ("%08x", 9),
        ("%#x", 11),
        ("%.12d", 14),
        ("%c", 2),
        ("%%", 2),
        ("%e", 15),
        ("%.3g", 12),
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
        "100%",
        "%300d",
    ],
)
def test_unbounded_formats(fmt: str) -> None:
    assert format_buffer_size(fmt) is None
