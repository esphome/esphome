"""Tests for the LVGL table widget's configuration validation."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.automation import ACTION_REGISTRY
from esphome.components.lvgl.widgets.table import (
    CONF_MERGE_RIGHT,
    CONF_TEXT_CROP,
    TABLE_SCHEMA,
)


def test_minimal_config_is_valid() -> None:
    assert TABLE_SCHEMA({}) == {}


def test_row_shorthand_expands_to_plain_cells() -> None:
    config = TABLE_SCHEMA({"rows": [["Name", "Value"]]})
    [row] = config["rows"]
    assert row["cells"] == [{"text": "Name"}, {"text": "Value"}]


def test_row_dict_form_with_cell_overrides() -> None:
    config = TABLE_SCHEMA(
        {
            "rows": [
                {
                    "cells": [
                        "Temp",
                        {"text": "22.5", "text_crop": True, "merge_right": True},
                    ]
                }
            ]
        }
    )
    [row] = config["rows"]
    assert row["cells"][0] == {"text": "Temp"}
    assert row["cells"][1] == {
        "text": "22.5",
        "merge_right": True,
        "text_crop": True,
    }


def test_row_count_defaults_are_not_injected_by_the_schema() -> None:
    # Inference of row/column counts from `rows` happens at code generation
    # time, not during validation - the schema should leave them unset.
    config = TABLE_SCHEMA({"rows": [["a", "b"], ["c"]]})
    assert "row_count" not in config
    assert "column_count" not in config


def test_explicit_row_and_column_count_are_kept() -> None:
    config = TABLE_SCHEMA({"row_count": 5, "column_count": 3})
    assert config["row_count"] == 5
    assert config["column_count"] == 3


def test_row_count_too_small_for_given_rows_raises() -> None:
    with pytest.raises(cv.Invalid, match="row_count"):
        TABLE_SCHEMA({"rows": [["a"], ["b"], ["c"]], "row_count": 2})


def test_column_count_too_small_for_given_cells_raises() -> None:
    with pytest.raises(cv.Invalid, match="column_count"):
        TABLE_SCHEMA({"rows": [["a", "b", "c"]], "column_count": 2})


def test_columns_list_longer_than_column_count_raises() -> None:
    with pytest.raises(cv.Invalid, match="columns"):
        TABLE_SCHEMA(
            {
                "column_count": 1,
                "columns": [{"width": 10}, {"width": 20}],
            }
        )


def test_columns_list_matching_inferred_column_count_is_valid() -> None:
    config = TABLE_SCHEMA(
        {
            "rows": [["a", "b"]],
            "columns": [{"width": 10}, {"width": 20}],
        }
    )
    assert [c["width"] for c in config["columns"]] == [10, 20]


@pytest.mark.parametrize(
    ("width", "expected"),
    [
        (100, 100),
        ("50%", 0.5),
        ("32px", 32),
    ],
)
def test_column_width_accepts_pixels_and_percent(width, expected) -> None:
    config = TABLE_SCHEMA({"columns": [{"width": width}]})
    assert config["columns"][0]["width"] == expected


def test_columns_percent_widths_summing_over_100_percent_raises() -> None:
    with pytest.raises(cv.Invalid, match="columns"):
        TABLE_SCHEMA({"columns": [{"width": "60%"}, {"width": "50%"}]})


def test_columns_percent_widths_summing_to_100_percent_is_valid() -> None:
    config = TABLE_SCHEMA({"columns": [{"width": "60%"}, {"width": "40%"}]})
    assert [c["width"] for c in config["columns"]] == [0.6, 0.4]


def test_columns_mixed_pixel_and_percent_widths_ignore_pixels_in_the_total() -> None:
    # Pixel widths aren't part of the percentage budget, so they shouldn't
    # count towards the 100% limit.
    config = TABLE_SCHEMA(
        {"columns": [{"width": 200}, {"width": "80%"}, {"width": "20%"}]}
    )
    assert [c["width"] for c in config["columns"]] == [200, 0.8, 0.2]


def test_selected_row_and_selected_column_are_independently_optional() -> None:
    config = TABLE_SCHEMA({"selected_row": 1})
    assert config["selected_row"] == 1
    assert "selected_column" not in config


def test_cell_update_action_requires_at_least_one_field() -> None:
    entry = ACTION_REGISTRY["lvgl.table.cell.update"]
    with pytest.raises(cv.Invalid):
        entry.schema({"id": "some_table", "row": 0, "column": 0})


def test_cell_update_action_accepts_a_single_field() -> None:
    entry = ACTION_REGISTRY["lvgl.table.cell.update"]
    config = entry.schema(
        {"id": "some_table", "row": 0, "column": 0, "merge_right": True}
    )
    assert config[CONF_MERGE_RIGHT] is True
    assert CONF_TEXT_CROP not in config
