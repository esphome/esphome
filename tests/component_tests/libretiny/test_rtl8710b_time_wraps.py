"""RTL8710B builds wrap the timing functions that rtl8710b_micros.c replaces; that file defines the wrappers only
for RTL8710B, so a flag on another family would fail the link."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.libretiny import RTL8710B_TIME_WRAPS
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "wrapped"),
    [
        ("rtl8710b.yaml", True),
        ("rtl8720c.yaml", False),
        ("ln882x.yaml", False),
        ("bk72xx.yaml", False),
        ("bk7238.yaml", False),
    ],
)
def test_rtl8710b_time_wraps(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    wrapped: bool,
) -> None:
    generate_main(component_config_path(config))
    for func in RTL8710B_TIME_WRAPS:
        assert (f"-Wl,--wrap={func}" in CORE.build_flags) is wrapped
