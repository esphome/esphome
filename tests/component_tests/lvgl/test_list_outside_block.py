"""Regression test for lvgl.list.add called from outside the lvgl: block.

lv_list.py's list_add_to_code() must call _register_lv_uses() and
_register_dynamic_widget_style_uses() before its first await (get_widgets(),
which can block until the target list is defined) -- for an action referenced
outside the lvgl: block, that wait can outlast lvgl's own to_code, which reads
get_lv_uses()/get_styles_used() and flushes everything they drive (USE_LVGL_*
defines, plus add_lv_use(image)/screen-transparency/A8-draw-support triggered
by style properties) just once, near the end of its run. Every existing
list_test.yaml call site lives inside lvgl: widgets:, so neither ordering
requirement had any coverage.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@dataclass
class GeneratedOutput:
    main_cpp: str
    define_names: set[str]
    lv_define_names: set[str]


@pytest.fixture(scope="module")
def generated(request: pytest.FixtureRequest) -> GeneratedOutput:
    config_path = (
        Path(request.fspath).parent / "config" / "list_outside_block_test.yaml"
    )
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        # Copy out before CORE.reset() below clears these out from under us.
        from esphome.components.lvgl import defines as df

        return GeneratedOutput(
            main_cpp=CORE.cpp_global_section + CORE.cpp_main_section,
            define_names={d.name for d in CORE.defines},
            lv_define_names=set(df.get_defines()),
        )
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_dynamic_widget_creates_correctly(generated: GeneratedOutput) -> None:
    assert (
        "lv_obj_t *dyn_switch_VAR_ = lv_switch_create(test_list);" in generated.main_cpp
    )


def test_dynamic_widget_type_use_define_is_registered(
    generated: GeneratedOutput,
) -> None:
    """The switch type is only ever referenced via the on_boot lvgl.list.add call
    (never declared as a static widget), so USE_LVGL_SWITCH can only be present
    if _register_lv_uses() ran in time for lvgl's own to_code to flush it.
    """
    assert "USE_LVGL_SWITCH" in generated.define_names
    assert "USE_LVGL_LIST" in generated.define_names


def test_dynamic_widget_style_use_defines_are_registered(
    generated: GeneratedOutput,
) -> None:
    """bg_image_src/transform_rotation/drop_shadow_color are only ever set on
    the dynamically-added switch (never on a static widget), so
    USE_LVGL_IMAGE/LV_COLOR_SCREEN_TRANSP/LV_DRAW_SW_SUPPORT_A8 can only be
    present if _register_dynamic_widget_style_uses() ran in time for lvgl's own
    to_code to flush them.
    """
    assert "USE_LVGL_IMAGE" in generated.define_names
    assert "LV_COLOR_SCREEN_TRANSP" in generated.lv_define_names
    assert "LV_DRAW_SW_SUPPORT_A8" in generated.lv_define_names
