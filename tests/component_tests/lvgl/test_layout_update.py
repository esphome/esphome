"""Tests for updating LVGL layout options via the update actions.

The ``lvgl.update`` and ``lvgl.widget.update`` (and per-widget
``lvgl.<widget>.update``) actions can change a container's layout *options* at
runtime. The layout ``type`` and the grid ``grid_rows``/``grid_columns``
structure are fixed at widget creation (they determine the cells/options
available to child widgets), so only the simple style options - those applied
via ``lv_obj_set_style_...`` calls - may be changed.

These tests cover both the ``layout_validator`` (schema/normalisation) and the
generated C++ for each target: a widget, the active screen (top-level
``lvgl.update``) and the display layers.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from voluptuous import Invalid

from esphome.__main__ import generate_cpp_contents
from esphome.components.lvgl.defines import TYPE_FLEX, TYPE_GRID, get_lv_uses
from esphome.components.lvgl.layout import layout_validator
from esphome.config import read_config
from esphome.core import CORE

# ---------------------------------------------------------------------------
# layout_validator - schema and normalisation
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "value,expected",
    [
        ({"flex_flow": "row"}, {"flex_flow": "LV_FLEX_FLOW_ROW"}),
        ({"flex_align_main": "center"}, {"flex_align_main": "LV_FLEX_ALIGN_CENTER"}),
        ({"flex_align_cross": "end"}, {"flex_align_cross": "LV_FLEX_ALIGN_END"}),
        (
            {"grid_column_align": "space_between"},
            {"grid_column_align": "LV_GRID_ALIGN_SPACE_BETWEEN"},
        ),
        ({"grid_row_align": "center"}, {"grid_row_align": "LV_GRID_ALIGN_CENTER"}),
        ({"pad_row": "7px"}, {"pad_row": 7}),
        ({"pad_column": "5px"}, {"pad_column": 5}),
    ],
)
def test_layout_validator_normalises_options(value: dict, expected: dict) -> None:
    """Each supported option is accepted and normalised to its LVGL form."""
    assert layout_validator(value) == expected


def test_layout_validator_accepts_multiple_options() -> None:
    """Several options may be combined in one update."""
    result = layout_validator(
        {"flex_flow": "column", "flex_align_main": "center", "pad_row": "4px"}
    )
    assert result == {
        "flex_flow": "LV_FLEX_FLOW_COLUMN",
        "flex_align_main": "LV_FLEX_ALIGN_CENTER",
        "pad_row": 4,
    }


@pytest.mark.parametrize(
    "value",
    [
        {"type": "flex"},
        {"type": "grid", "grid_column_align": "center"},
        {"grid_rows": 3},
        {"grid_columns": ["fr(1)"]},
        {"grid_rows": [1, 2], "flex_flow": "row"},
    ],
)
def test_layout_validator_rejects_structural_keys(value: dict) -> None:
    """The layout type and grid structure are fixed at creation and must not
    be changeable via an update action."""
    with pytest.raises(Invalid, match="extra keys not allowed"):
        layout_validator(value)


def test_layout_validator_rejects_empty() -> None:
    """An update must specify at least one layout option."""
    with pytest.raises(Invalid, match="at least one layout option"):
        layout_validator({})


def test_layout_validator_registers_flex_use() -> None:
    """Validating a flex option registers the flex feature so LV_USE_FLEX is
    emitted even when the option is set solely via an update action."""
    layout_validator({"flex_flow": "row"})
    assert TYPE_FLEX in get_lv_uses()


def test_layout_validator_registers_grid_use() -> None:
    """Validating a grid option registers the grid feature."""
    layout_validator({"grid_column_align": "center"})
    assert TYPE_GRID in get_lv_uses()


def test_pad_only_update_registers_no_layout_use() -> None:
    """Padding options belong to both layout types, so they alone do not force
    either feature on."""
    layout_validator({"pad_row": "4px"})
    uses = get_lv_uses()
    assert TYPE_FLEX not in uses
    assert TYPE_GRID not in uses


# ---------------------------------------------------------------------------
# Generated C++ for the update actions
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    """Generate the C++ output for the shared layout-update YAML config once
    per module (codegen is relatively expensive)."""
    config_path = Path(request.fspath).parent / "config" / "layout_update_test.yaml"
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_widget_flex_update_applies_partial_options(main_cpp: str) -> None:
    """``lvgl.widget.update`` changes only the flex options that are specified,
    via the appropriate ``lv_obj_set_style_...``/``lv_obj_set_flex_flow``
    calls on the target widget."""
    assert "lv_obj_set_flex_flow(flex_box, LV_FLEX_FLOW_COLUMN)" in main_cpp
    assert (
        "lv_obj_set_style_flex_main_place(flex_box, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT)"
        in main_cpp
    )
    assert (
        "lv_obj_set_style_flex_cross_place(flex_box, LV_FLEX_ALIGN_END, LV_STATE_DEFAULT)"
        in main_cpp
    )
    assert "lv_obj_set_style_pad_row(flex_box, 7, LV_STATE_DEFAULT)" in main_cpp


def test_widget_flex_update_does_not_change_type(main_cpp: str) -> None:
    """The update must not re-establish the layout type: ``lv_obj_set_layout``
    is emitted once (at creation) and never from the update action."""
    assert main_cpp.count("lv_obj_set_layout(flex_box,") == 1


def test_widget_flex_update_is_partial(main_cpp: str) -> None:
    """An option that was not specified in the update (the track placement) is
    only set at creation, not by the partial update."""
    assert main_cpp.count("lv_obj_set_style_flex_track_place(flex_box,") == 1


def test_widget_grid_update_applies_alignments(main_cpp: str) -> None:
    """``lvgl.widget.update`` on a grid container changes its alignment
    options without touching the grid structure."""
    assert (
        "lv_obj_set_style_grid_column_align(grid_box, LV_GRID_ALIGN_SPACE_BETWEEN, "
        "LV_STATE_DEFAULT)" in main_cpp
    )
    assert (
        "lv_obj_set_style_grid_row_align(grid_box, LV_GRID_ALIGN_CENTER, LV_STATE_DEFAULT)"
        in main_cpp
    )


def test_grid_update_does_not_regenerate_descriptor_arrays(main_cpp: str) -> None:
    """The grid row/column descriptor arrays are structural and generated once
    at creation; an update must not regenerate them."""
    assert main_cpp.count("grid_box_row_dsc") != 0
    # The descriptor array is declared once and referenced once at creation.
    assert main_cpp.count("grid_box_row_dsc") == main_cpp.count("grid_box_column_dsc")
    assert "lv_obj_set_layout(grid_box," in main_cpp
    assert main_cpp.count("lv_obj_set_layout(grid_box,") == 1


def test_top_level_layout_targets_active_screen(main_cpp: str) -> None:
    """A top-level ``lvgl.update: { layout: ... }`` applies to the active
    screen, not to the LVGL component object."""
    assert (
        "lv_obj_set_flex_flow(lvgl_id->get_screen_active(), LV_FLEX_FLOW_COLUMN)"
        in main_cpp
    )
    assert (
        "lv_obj_set_style_pad_column(lvgl_id->get_screen_active(), 5, LV_STATE_DEFAULT)"
        in main_cpp
    )


def test_top_layer_layout_applied(main_cpp: str) -> None:
    """A layout under ``top_layer`` is applied to the display's top layer."""
    assert "lv_display_get_layer_top(lvgl_id->get_disp())" in main_cpp
    assert "lv_obj_set_flex_flow(top_layer_VAR_, LV_FLEX_FLOW_ROW)" in main_cpp


def test_bottom_layer_styling_applied(main_cpp: str) -> None:
    """A ``bottom_layer`` style update generates code (previously the layer
    keys of ``lvgl.update`` were silently ignored)."""
    assert "lv_display_get_layer_bottom(lvgl_id->get_disp())" in main_cpp
    assert (
        "lv_obj_set_style_bg_color(bottom_layer_VAR_, lv_color_make(18, 52, 86), "
        "LV_PART_MAIN)" in main_cpp
    )
