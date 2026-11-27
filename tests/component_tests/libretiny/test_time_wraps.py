"""RTL8710B and RTL8720C builds wrap the timing functions their rtl87*_micros.c replaces and compile only that file;
other families get neither, since a flag without the wrappers fails the link."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.libretiny import (
    FILTER_SOURCE_FILES,
    TIME_WRAPS,
    time_wrap_source,
)
from esphome.components.libretiny.const import (
    FAMILY_RTL8710B,
    FAMILY_RTL8720C,
    KEY_LIBRETINY,
)
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "family"),
    [
        ("rtl8710b.yaml", FAMILY_RTL8710B),
        ("rtl8720c.yaml", FAMILY_RTL8720C),
        ("ln882x.yaml", None),
        ("bk72xx.yaml", None),
        ("bk7238.yaml", None),
    ],
)
def test_time_wraps(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    family: str | None,
) -> None:
    generate_main(component_config_path(config))
    wrapped = TIME_WRAPS.get(family, ())
    for funcs in TIME_WRAPS.values():
        for func in funcs:
            assert (f"-Wl,--wrap={func}" in CORE.build_flags) is (func in wrapped)
    excluded = FILTER_SOURCE_FILES()
    for other in TIME_WRAPS:
        assert (time_wrap_source(other) in excluded) is (other != family)


@pytest.mark.parametrize("core_data", [{}, {KEY_LIBRETINY: {}}])
def test_nothing_excluded_before_the_family_is_known(core_data: dict) -> None:
    with patch("esphome.components.libretiny.CORE.data", core_data):
        assert FILTER_SOURCE_FILES() == []
