"""Updating a widget whose update schema has no widget-specific options must not re-run
the creation handler, which reads keys that only exist at creation time."""

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
        / "update_creation_only_widgets_test.yaml"
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


@pytest.mark.parametrize(
    ("widget", "creation_call"),
    [
        ("test_tabview", "lv_tabview_add_tab("),
        ("test_tileview", "lv_tileview_add_tile("),
        ("test_canvas", "lv_canvas_set_draw_buf("),
        ("test_menu", "lv_menu_set_page("),
    ],
)
def test_update_sets_flag_without_recreating(
    main_cpp: str, widget: str, creation_call: str
) -> None:
    assert f"lv_obj_set_hidden({widget}, true);" in main_cpp
    assert main_cpp.count(creation_call) == 1
