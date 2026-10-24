"""Canvas draw options must map onto the LVGL 9 draw descriptor fields."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.__main__ import generate_cpp_contents
from esphome.automation import ACTION_REGISTRY
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
        return CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_rect_gradient_is_copied_by_value(main_cpp: str) -> None:
    assert ".bg_grad = *test_grad;" in main_cpp


def test_line_dash_fields(main_cpp: str) -> None:
    assert ".dash_width = 4;" in main_cpp
    assert ".dash_gap = 2;" in main_cpp


@pytest.mark.parametrize(
    ("action", "config"),
    [
        ("lvgl.canvas.draw_rectangle", {"x": 0, "y": 0, "width": 10, "height": 10}),
        ("lvgl.canvas.draw_polygon", {"points": [{"x": 0, "y": 0}]}),
    ],
)
def test_rect_opa_names_the_per_part_keys(
    main_cpp: str, action: str, config: dict
) -> None:
    with pytest.raises(cv.Invalid, match="bg_opa"):
        ACTION_REGISTRY[action].schema({"id": "cv", "opa": "50%", **config})
