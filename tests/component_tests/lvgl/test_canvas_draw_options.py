"""Canvas draw options must map onto the LVGL 9 draw descriptor fields."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.__main__ import generate_cpp_contents
from esphome.automation import ACTION_REGISTRY
import esphome.components.lvgl.widgets.canvas  # noqa: F401 - registers the actions
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = (
        Path(request.fspath).parent / "config" / "canvas_draw_options_test.yaml"
    )
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_rect_gradient_is_copied_by_value(main_cpp: str) -> None:
    assert ".bg_grad = *test_grad;" in main_cpp


def test_line_dash_fields(main_cpp: str) -> None:
    assert ".dash_width = 4;" in main_cpp
    assert ".dash_gap = 2;" in main_cpp


def test_rect_opa_names_the_per_part_keys() -> None:
    with pytest.raises(cv.Invalid, match="bg_opa"):
        ACTION_REGISTRY["lvgl.canvas.draw_rectangle"].schema(
            {"id": "cv", "x": 0, "y": 0, "width": 10, "height": 10, "opa": "50%"}
        )


@pytest.mark.parametrize(
    "key", ["opa", "bg_color", "bg_grad", "radius", "shadow_width"]
)
def test_polygon_rejects_non_border_keys(key: str) -> None:
    with pytest.raises(cv.Invalid, match="drawn as an outline"):
        ACTION_REGISTRY["lvgl.canvas.draw_polygon"].schema(
            {"id": "cv", "points": [{"x": 0, "y": 0}], key: 1}
        )


def test_polygon_accepts_border_keys() -> None:
    config = ACTION_REGISTRY["lvgl.canvas.draw_polygon"].schema(
        {
            "id": "cv",
            "points": [{"x": 0, "y": 0}],
            "border_color": 0xFF0000,
            "border_width": 2,
            "border_opa": "50%",
        }
    )
    assert config["border_width"] == 2


def test_computed_text_outlives_the_layer(main_cpp: str) -> None:
    # The layer draws the label when it is finished, so the computed text is held, without a copy,
    # in a reference declared before the layer and used for the descriptor
    text = main_cpp.index("const std::string &text_VAR_ = ")
    layer = main_cpp.index("lv_canvas_init_layer(", text)
    assign = main_cpp.index("dsc_VAR_.text = text_VAR_.c_str();", layer)
    main_cpp.index("lv_canvas_finish_layer(", assign)


def test_conditional_text_is_copied_until_the_layer_is_drawn(main_cpp: str) -> None:
    # An if_nan text picks between a temporary's c_str() and a literal, so it is copied
    text = main_cpp.index("const std::string text_VAR_ = (std::isfinite(")
    layer = main_cpp.index("lv_canvas_init_layer(", text)
    main_cpp.index("dsc_VAR_.text = text_VAR_.c_str();", layer)
