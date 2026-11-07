"""LN882x builds with 1-byte enums to match the SDK's prebuilt libraries;
the other LibreTiny families keep LibreTiny's own enum size."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_ln882x_swaps_in_short_enums(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("ln882x.yaml"))
    assert "-fno-short-enums" in CORE.build_unflags
    assert "-fshort-enums" in CORE.build_flags


def test_bk72xx_keeps_libretiny_enums(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("bk72xx.yaml"))
    assert "-fno-short-enums" not in CORE.build_unflags
    assert "-fshort-enums" not in CORE.build_flags
