"""Strobe and addressable_color_wipe colors live in flash tables; random wipe colors get RAM slots."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest

import esphome.config_validation as cv


def test_effect_colors_use_flash_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("effect_color_tables.yaml"))

    assert (
        "static constexpr light::StrobeLightEffectColor light_strobe_colors[] PROGMEM"
        in main_cpp
    )
    # Both strobes use the same steps, so they share one table
    assert main_cpp.count("->set_colors(light_strobe_colors, 2);") == 2
    assert re.search(
        r"static constexpr light::AddressableColorWipeEffectColor light_color_wipe_colors\w*\[\] PROGMEM",
        main_cpp,
    )
    # Only the random entry gets a RAM slot, starting at its configured color
    assert re.search(r"static Color \w+_random_colors\[\] = \{Color\(", main_cpp)
    assert ".random_slot = 0," in main_cpp
    assert ".random_slot = light::NO_RANDOM_SLOT," in main_cpp


def _color_wipe_colors(colors: list[dict]) -> list[dict]:
    from esphome.components.light.effects import EFFECTS_REGISTRY

    entry = EFFECTS_REGISTRY["addressable_color_wipe"]
    return entry.schema({"name": "Wipe", "colors": colors})["colors"]


def test_color_wipe_requires_a_color() -> None:
    with pytest.raises(cv.Invalid):
        _color_wipe_colors([])


def test_color_wipe_random_entry_limit() -> None:
    random_color = {"random": True, "num_leds": 1}
    assert len(_color_wipe_colors([random_color] * 255)) == 255
    with pytest.raises(cv.Invalid, match="At most 255 colors can be random"):
        _color_wipe_colors([random_color] * 256)
