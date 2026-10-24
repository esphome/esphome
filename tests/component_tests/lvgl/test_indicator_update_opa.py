"""lvgl.indicator.update must apply opa to arc sections and needle objects."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = (
        Path(request.fspath).parent / "config" / "indicator_update_opa_test.yaml"
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


def test_arc_section_opa_is_applied(main_cpp: str) -> None:
    assert "lv_style_set_arc_opa(meter_arc_test_arc_style, " in main_cpp
    assert "lv_obj_invalidate(" in main_cpp


def test_needle_opa_is_applied(main_cpp: str) -> None:
    assert "lv_obj_set_style_opa(test_needle" in main_cpp
