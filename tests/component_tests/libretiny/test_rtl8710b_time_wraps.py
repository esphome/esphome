"""RTL8710B builds wrap the timing functions that rtl8710b_micros.c replaces and compile that file; other families
get neither, since a flag without the wrappers fails the link."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.libretiny import FILTER_SOURCE_FILES, RTL8710B_TIME_WRAPS
from esphome.components.libretiny.const import KEY_LIBRETINY
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
    assert ("rtl8710b_micros.c" not in FILTER_SOURCE_FILES()) is wrapped


@pytest.mark.parametrize("core_data", [{}, {KEY_LIBRETINY: {}}])
def test_nothing_excluded_before_the_family_is_known(core_data: dict) -> None:
    with patch("esphome.components.libretiny.CORE.data", core_data):
        assert FILTER_SOURCE_FILES() == []
