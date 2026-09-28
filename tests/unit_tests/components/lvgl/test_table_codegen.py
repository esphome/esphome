"""Tests for the LVGL table widget's C++ code generation."""

from __future__ import annotations

import pytest

from esphome.automation import ACTION_REGISTRY
from esphome.components.lvgl.defines import set_widgets_completed
from esphome.components.lvgl.lvcode import LvContext
from esphome.components.lvgl.schemas import container_schema
from esphome.components.lvgl.trigger import generate_triggers
from esphome.components.lvgl.widgets import Widget, widget_to_code
from esphome.components.lvgl.widgets.table import table_spec
from esphome.const import (
    CONF_AUTOMATION_ID,
    CONF_ON_VALUE,
    CONF_THEN,
    CONF_TRIGGER_ID,
    CONF_TYPE_ID,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, TemplateArguments
from esphome.yaml_util import make_data_base


async def _create_table(raw_config: dict) -> Widget:
    """Validate `raw_config` as a table widget and generate its creation code."""
    config = container_schema(table_spec)(raw_config)
    parent = MockObj("parent_obj")
    async with LvContext():
        return await widget_to_code(config, table_spec, parent)


def _statements() -> list[str]:
    return [str(s) for s in CORE.main_statements]


@pytest.mark.asyncio
async def test_create_table_sets_row_and_column_count(setup_core) -> None:
    await _create_table(
        {"id": "table_counts", "rows": [["Name", "Value"], ["Temp", "22.5"]]}
    )
    statements = _statements()
    assert any("lv_table_set_row_count(table_counts->obj, 2)" in s for s in statements)
    assert any(
        "lv_table_set_column_count(table_counts->obj, 2)" in s for s in statements
    )


@pytest.mark.asyncio
async def test_create_table_writes_cell_values(setup_core) -> None:
    await _create_table({"id": "table_cells", "rows": [["Name", "Value"]]})
    statements = _statements()
    assert any(
        'lv_table_set_cell_value(table_cells->obj, 0, 0, "Name")' in s
        for s in statements
    )
    assert any(
        'lv_table_set_cell_value(table_cells->obj, 0, 1, "Value")' in s
        for s in statements
    )


@pytest.mark.asyncio
async def test_create_table_sets_cell_control_flags(setup_core) -> None:
    await _create_table(
        {
            "id": "table_ctrl",
            "rows": [
                {
                    "cells": [
                        {"text": "wide", "merge_right": True},
                        {"text": "cropped", "text_crop": True},
                    ]
                }
            ],
        }
    )
    statements = _statements()
    assert any(
        "lv_table_set_cell_ctrl(table_ctrl->obj, 0, 0, LV_TABLE_CELL_CTRL_MERGE_RIGHT)"
        in s
        for s in statements
    )
    assert any(
        "lv_table_set_cell_ctrl(table_ctrl->obj, 0, 1, LV_TABLE_CELL_CTRL_TEXT_CROP)"
        in s
        for s in statements
    )
    # text_crop omitted for cell 0: no clear_cell_ctrl() should be emitted.
    assert not any(
        "table_ctrl->obj, 0, 0, LV_TABLE_CELL_CTRL_TEXT_CROP" in s for s in statements
    )


@pytest.mark.asyncio
async def test_pixel_column_width_calls_lvgl_directly(setup_core) -> None:
    await _create_table({"id": "table_px", "columns": [{"width": 96}]})
    statements = _statements()
    assert any(
        "lv_table_set_column_width(table_px->obj, 0, 96)" in s for s in statements
    )


@pytest.mark.asyncio
async def test_percent_column_width_uses_the_dynamic_helper(setup_core) -> None:
    """Regression test: lv_table_set_column_width() only accepts a literal
    pixel count, so a percentage width must not be passed to it directly -
    it has to go through the LvTableType helper that recomputes it at
    runtime from the table's actual content width.
    """
    await _create_table({"id": "table_pct", "columns": [{"width": "40%"}]})
    statements = _statements()
    assert any("table_pct->init_column_pct(1)" in s for s in statements)
    assert any("table_pct->add_column_width_pct(0, 40)" in s for s in statements)
    assert not any(
        "lv_table_set_column_width(table_pct->obj, 0" in s for s in statements
    )


@pytest.mark.asyncio
async def test_selected_cell_with_both_indices(setup_core) -> None:
    await _create_table(
        {"id": "table_sel_both", "selected_row": 1, "selected_column": 2}
    )
    statements = _statements()
    assert any(
        "lv_table_set_selected_cell(table_sel_both->obj, 1, 2)" in s for s in statements
    )


@pytest.mark.asyncio
async def test_selected_cell_with_only_row_selects_whole_row(setup_core) -> None:
    await _create_table({"id": "table_sel_row", "selected_row": 1})
    statements = _statements()
    assert any(
        "lv_table_set_selected_cell(table_sel_row->obj, 1, LV_TABLE_CELL_NONE)" in s
        for s in statements
    )


@pytest.mark.asyncio
async def test_selected_cell_omitted_entirely_when_not_configured(
    setup_core,
) -> None:
    await _create_table({"id": "table_no_selection", "rows": [["a"]]})
    statements = _statements()
    assert not any("lv_table_set_selected_cell" in s for s in statements)


@pytest.mark.asyncio
async def test_cell_update_action_writes_only_the_given_fields(setup_core) -> None:
    await _create_table({"id": "table_update", "rows": [["a", "b"], ["c", "d"]]})
    set_widgets_completed(True)
    # Only inspect statements emitted by the action below, not by creation.
    before = len(_statements())

    entry = ACTION_REGISTRY["lvgl.table.cell.update"]
    config = entry.schema(
        {"id": "table_update", "row": 1, "column": 1, "text": "new value"}
    )
    action_id = ID("test_cell_update_action", is_declaration=True, type=entry.type_id)
    await entry.coroutine_fun(config, action_id, TemplateArguments(), [])

    statements = _statements()[before:]
    assert any(
        'lv_table_set_cell_value(table_update->obj, 1, 1, "new value")' in s
        for s in statements
    )
    # Neither control flag was specified, so neither call should be emitted.
    assert not any("LV_TABLE_CELL_CTRL" in s for s in statements)


@pytest.mark.asyncio
async def test_on_value_registers_a_value_changed_event_callback(setup_core) -> None:
    config = container_schema(table_spec)(
        {
            "id": "table_on_value",
            "rows": [["a"]],
            "on_value": [
                {"lambda": make_data_base("id(table_on_value).get_selected_row();")}
            ],
        }
    )
    # Auto-generated IDs (trigger/automation/action) are normally resolved to
    # unique names by esphome's full config pass before code generation; do
    # that by hand here since this test only exercises the widget/trigger
    # codegen slice in isolation.
    automation_conf = config[CONF_ON_VALUE][0]
    automation_conf[CONF_TRIGGER_ID].resolve([])
    automation_conf[CONF_AUTOMATION_ID].resolve([])
    automation_conf[CONF_THEN][0][CONF_TYPE_ID].resolve([])

    parent = MockObj("parent_obj")
    async with LvContext():
        await widget_to_code(config, table_spec, parent)
        set_widgets_completed(True)
        await generate_triggers()

    statements = _statements()
    assert any(
        "table_on_value->obj" in s
        and "add_event_cb" in s
        and "LV_EVENT_VALUE_CHANGED" in s
        for s in statements
    )
