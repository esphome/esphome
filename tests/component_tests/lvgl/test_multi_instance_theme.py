"""Regression test: a widget's theme style must be attached regardless of
which LVGL instance declares theme: and which instance's widgets are built
first.

theme_to_code() runs once per LVGL instance, interleaved with that instance's
own add_widgets(). A widget built by an earlier instance, before any instance
has declared theming for its type, must still pick up a later instance's
theme: declaration for that type - including on the later instance's own
widgets, which get_widget_theme_styles()'s memoisation can poison too.
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
        Path(request.fspath).parent / "config" / "multi_instance_theme_test.yaml"
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


def test_earlier_instance_widget_is_themed(main_cpp: str) -> None:
    assert "lv_obj_add_style(label_0, _lv_theme_style_label_main_default," in main_cpp


def test_later_instance_widget_is_themed(main_cpp: str) -> None:
    assert "lv_obj_add_style(label_1, _lv_theme_style_label_main_default," in main_cpp
