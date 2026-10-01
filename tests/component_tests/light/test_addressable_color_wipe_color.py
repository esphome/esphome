"""Regression test: addressable_color_wipe's `color:` must scale by color_brightness.

The per-item schema tests in test_effect_color.py only check the validated config
dict; they would still pass if addressable_color_wipe_effect_to_code dropped
color_brightness on the floor (as it originally did), since a dark color's red/green/
blue are already peak-normalized to 1.0 by the time codegen sees them. Generate the
actual C++ and check the AddressableColorWipeEffectColor byte values instead, so a
regression in the scaling multiplication itself is caught.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = (
        Path(request.fspath).parent
        / "config"
        / "addressable_color_wipe_color_test.yaml"
    )
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_dark_color_name_is_scaled_by_color_brightness(main_cpp: str) -> None:
    # color: darkred (0x8B0000) normalizes to r=1.0 but color_brightness=0x8B/0xFF,
    # so the byte value must come back down to 0x8B (139), not 255.
    assert ".r = 139," in main_cpp
    assert ".g = 0," in main_cpp
    assert ".b = 0," in main_cpp


def test_explicit_channel_is_unaffected(main_cpp: str) -> None:
    # red: 50% with no color: or color_brightness: set must still scale by the
    # default color_brightness of 1.0, i.e. come out as plain 50% of 255.
    assert ".r = 128," in main_cpp
