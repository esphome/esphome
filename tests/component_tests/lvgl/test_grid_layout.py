"""Unit tests for the LVGL grid layout shorthand and rows/columns auto-sizing."""

from __future__ import annotations

import pytest
from voluptuous import Invalid

from esphome.components.lvgl.defines import (
    CONF_GRID_COLUMNS,
    CONF_GRID_ROWS,
    CONF_LAYOUT,
    CONF_WIDGETS,
    TYPE_GRID,
)
from esphome.components.lvgl.layout import GridLayout, grid_dimension
from esphome.const import CONF_TYPE

FR1 = "LV_GRID_FR(1)"


def _widgets(n: int) -> list[dict]:
    """Build a list of `n` placeholder widgets for the validate() input."""
    return [{"label": {}} for _ in range(n)]


# ---------------------------------------------------------------------------
# grid_dimension validator
# ---------------------------------------------------------------------------


def test_grid_dimension_int_expands_to_fr1_list() -> None:
    """A positive integer should expand to a list of LV_GRID_FR(1) entries."""
    assert grid_dimension(1) == [FR1]
    assert grid_dimension(3) == [FR1, FR1, FR1]


def test_grid_dimension_zero_or_negative_rejected() -> None:
    """Non-positive integers must be rejected."""
    with pytest.raises(Invalid):
        grid_dimension(0)
    with pytest.raises(Invalid):
        grid_dimension(-2)


def test_grid_dimension_list_passes_through() -> None:
    """A list should be validated through the existing grid_spec list schema."""
    result = grid_dimension(["100px", "content", "fr(2)"])
    # `grid_spec` normalises each entry: pixel sizes become ints, the
    # CONTENT keyword is uppercased and prefixed, and FR(n) is normalised.
    assert result == [100, "LV_GRID_CONTENT", "LV_GRID_FR(2)"]


def test_grid_dimension_invalid_string_rejected() -> None:
    """A string is not a valid grid dimension and should be rejected."""
    with pytest.raises(Invalid):
        grid_dimension("not a list")


def test_grid_dimension_empty_list_rejected() -> None:
    """An empty list of grid specs must be rejected."""
    with pytest.raises(Invalid, match="at least one entry"):
        grid_dimension([])


# ---------------------------------------------------------------------------
# Shorthand string layouts
# ---------------------------------------------------------------------------


def test_shorthand_full_form_unchanged() -> None:
    """`<rows>x<cols>` continues to work and yields the exact dimensions."""
    config = {CONF_LAYOUT: "2x3", CONF_WIDGETS: _widgets(0)}
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    assert layout[CONF_TYPE] == TYPE_GRID
    assert layout[CONF_GRID_ROWS] == [FR1, FR1]
    assert layout[CONF_GRID_COLUMNS] == [FR1, FR1, FR1]


def test_shorthand_rows_only_calculates_columns_from_widgets() -> None:
    """`<rows>x` derives the column count from the number of widgets."""
    config = {CONF_LAYOUT: "3x", CONF_WIDGETS: _widgets(7)}
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    # 7 widgets / 3 rows -> ceil = 3 columns.
    assert len(layout[CONF_GRID_ROWS]) == 3
    assert len(layout[CONF_GRID_COLUMNS]) == 3


def test_shorthand_columns_only_calculates_rows_from_widgets() -> None:
    """`x<cols>` derives the row count from the number of widgets."""
    config = {CONF_LAYOUT: "x4", CONF_WIDGETS: _widgets(5)}
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    # 5 widgets / 4 cols -> ceil = 2 rows.
    assert len(layout[CONF_GRID_ROWS]) == 2
    assert len(layout[CONF_GRID_COLUMNS]) == 4


def test_shorthand_rows_only_no_widgets_defaults_columns_to_one() -> None:
    """With no widgets and only rows specified, the column count defaults to 1."""
    config = {CONF_LAYOUT: "3x", CONF_WIDGETS: []}
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    assert len(layout[CONF_GRID_ROWS]) == 3
    assert len(layout[CONF_GRID_COLUMNS]) == 1


def test_shorthand_columns_only_no_widgets_defaults_rows_to_one() -> None:
    """With no widgets and only columns specified, the row count defaults to 1."""
    config = {CONF_LAYOUT: "x4", CONF_WIDGETS: []}
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    assert len(layout[CONF_GRID_ROWS]) == 1
    assert len(layout[CONF_GRID_COLUMNS]) == 4


def test_shorthand_with_whitespace_accepted() -> None:
    """The shorthand parser should tolerate whitespace around the components."""
    config = {CONF_LAYOUT: "  3 x ", CONF_WIDGETS: _widgets(6)}
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    # 6 widgets / 3 rows -> 2 columns.
    assert len(layout[CONF_GRID_ROWS]) == 3
    assert len(layout[CONF_GRID_COLUMNS]) == 2


def test_shorthand_bare_x_rejected() -> None:
    """Pure `x` (no digits at all) is not a valid shorthand."""
    config = {CONF_LAYOUT: "x", CONF_WIDGETS: _widgets(2)}
    with pytest.raises(Invalid):
        GridLayout().validate(config)


@pytest.mark.parametrize(
    "layout,bad_label",
    [
        ("0x3", "row"),
        ("3x0", "column"),
        ("0x", "row"),
        ("x0", "column"),
        ("0x0", "row"),
    ],
)
def test_shorthand_zero_dimension_rejected(layout: str, bad_label: str) -> None:
    """Shorthand row/column counts must be >= 1."""
    config = {CONF_LAYOUT: layout, CONF_WIDGETS: _widgets(2)}
    with pytest.raises(Invalid, match=f"{bad_label} count must be at least 1"):
        GridLayout().validate(config)


def test_shorthand_get_layout_schemas_recognizes_partial_forms() -> None:
    """`<rows>x` and `x<cols>` should be picked up by GridLayout.get_layout_schemas."""
    grid = GridLayout()
    for layout in ("3x", "x4", "2x3"):
        layout_schema, _ = grid.get_layout_schemas({CONF_LAYOUT: layout})
        assert layout_schema is not None, f"{layout!r} should be recognised"
    # Pure `x` and unrelated strings should not be picked up as a grid layout.
    for layout in ("x", "horizontal"):
        layout_schema, _ = grid.get_layout_schemas({CONF_LAYOUT: layout})
        assert layout_schema is None, f"{layout!r} should not be recognised"


# ---------------------------------------------------------------------------
# Dict-form layouts with rows/columns auto-sizing
# ---------------------------------------------------------------------------


def test_dict_rows_only_calculates_columns_from_widgets() -> None:
    """A dict layout with only rows fills in the column count from widget count."""
    config = {
        CONF_LAYOUT: {
            CONF_TYPE: TYPE_GRID,
            CONF_GRID_ROWS: [FR1, FR1],
        },
        CONF_WIDGETS: _widgets(5),
    }
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    # 5 widgets / 2 rows -> ceil = 3 columns.
    assert len(layout[CONF_GRID_ROWS]) == 2
    assert layout[CONF_GRID_COLUMNS] == [FR1, FR1, FR1]


def test_dict_columns_only_calculates_rows_from_widgets() -> None:
    """A dict layout with only columns fills in the row count from widget count."""
    config = {
        CONF_LAYOUT: {
            CONF_TYPE: TYPE_GRID,
            CONF_GRID_COLUMNS: [FR1, FR1, FR1],
        },
        CONF_WIDGETS: _widgets(7),
    }
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    # 7 widgets / 3 cols -> ceil = 3 rows.
    assert layout[CONF_GRID_ROWS] == [FR1, FR1, FR1]
    assert len(layout[CONF_GRID_COLUMNS]) == 3


def test_dict_rows_only_no_widgets_defaults_columns_to_one() -> None:
    """A dict layout with rows but no widgets defaults columns to 1."""
    config = {
        CONF_LAYOUT: {
            CONF_TYPE: TYPE_GRID,
            CONF_GRID_ROWS: [FR1, FR1, FR1],
        },
        CONF_WIDGETS: [],
    }
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    assert len(layout[CONF_GRID_ROWS]) == 3
    assert layout[CONF_GRID_COLUMNS] == [FR1]


def test_dict_neither_rows_nor_columns_rejected() -> None:
    """A grid layout dict without rows AND without columns must be rejected."""
    config = {
        CONF_LAYOUT: {CONF_TYPE: TYPE_GRID},
        CONF_WIDGETS: _widgets(3),
    }
    with pytest.raises(Invalid):
        GridLayout().validate(config)


def test_dict_both_rows_and_columns_unchanged() -> None:
    """When both dimensions are present they are preserved as-is."""
    config = {
        CONF_LAYOUT: {
            CONF_TYPE: TYPE_GRID,
            CONF_GRID_ROWS: [FR1, FR1],
            CONF_GRID_COLUMNS: [FR1, FR1, FR1],
        },
        CONF_WIDGETS: _widgets(0),
    }
    result = GridLayout().validate(config)
    layout = result[CONF_LAYOUT]
    assert layout[CONF_GRID_ROWS] == [FR1, FR1]
    assert layout[CONF_GRID_COLUMNS] == [FR1, FR1, FR1]
