"""RTL8710B and RTL8720C builds wrap the timing functions their rtl87*_micros.c replaces and compile only that file;
other families get neither, since a flag without the wrappers fails the link."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.libretiny import FILTER_SOURCE_FILES
from esphome.components.libretiny.const import (
    FAMILY_RTL8710B,
    FAMILY_RTL8720C,
    KEY_LIBRETINY,
)
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "family", "expected"),
    [
        (
            "rtl8710b.yaml",
            FAMILY_RTL8710B,
            {"millis", "micros", "delay", "delayMicroseconds"},
        ),
        ("rtl8720c.yaml", FAMILY_RTL8720C, {"micros", "delayMicroseconds"}),
        ("ln882x.yaml", None, set()),
        ("bk72xx.yaml", None, set()),
        ("bk7238.yaml", None, set()),
    ],
)
def test_time_wraps(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    family: str | None,
    expected: set[str],
) -> None:
    generate_main(component_config_path(config))
    for func in ("millis", "micros", "delay", "delayMicroseconds"):
        assert (f"-Wl,--wrap={func}" in CORE.build_flags) is (func in expected)
    excluded = FILTER_SOURCE_FILES()
    for source in ("rtl8710b_micros.c", "rtl8720c_micros.c"):
        assert (source in excluded) is (
            family is None or source != f"{family.lower()}_micros.c"
        )


@pytest.mark.parametrize("core_data", [{}, {KEY_LIBRETINY: {}}])
def test_nothing_excluded_before_the_family_is_known(core_data: dict) -> None:
    with patch("esphome.components.libretiny.CORE.data", core_data):
        assert FILTER_SOURCE_FILES() == []
