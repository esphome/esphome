import math
import re
import textwrap

import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_TYPE, CONF_WIDTH

from .defines import (
    CONF_FLEX_ALIGN_CROSS,
    CONF_FLEX_ALIGN_MAIN,
    CONF_FLEX_ALIGN_TRACK,
    CONF_FLEX_FLOW,
    CONF_FLEX_GROW,
    CONF_GRID_CELL_COLUMN_POS,
    CONF_GRID_CELL_COLUMN_SPAN,
    CONF_GRID_CELL_ROW_POS,
    CONF_GRID_CELL_ROW_SPAN,
    CONF_GRID_CELL_X_ALIGN,
    CONF_GRID_CELL_Y_ALIGN,
    CONF_GRID_COLUMN_ALIGN,
    CONF_GRID_COLUMNS,
    CONF_GRID_ROW_ALIGN,
    CONF_GRID_ROWS,
    CONF_LAYOUT,
    CONF_PAD_COLUMN,
    CONF_PAD_ROW,
    CONF_WIDGETS,
    FLEX_FLOWS,
    LV_CELL_ALIGNMENTS,
    LV_FLEX_ALIGNMENTS,
    LV_FLEX_CROSS_ALIGNMENTS,
    LV_GRID_ALIGNMENTS,
    TYPE_FLEX,
    TYPE_GRID,
    TYPE_NONE,
    LvConstant,
)
from .lv_validation import padding, size

CONF_MULTIPLE_WIDGETS_PER_CELL = "multiple_widgets_per_cell"

cell_alignments = LV_CELL_ALIGNMENTS.one_of
grid_alignments = LV_GRID_ALIGNMENTS.one_of
flex_alignments = LV_FLEX_ALIGNMENTS.one_of

FLEX_LAYOUT_SCHEMA = {
    cv.Required(CONF_TYPE): cv.one_of(TYPE_FLEX, lower=True),
    cv.Optional(CONF_FLEX_FLOW, default="row_wrap"): FLEX_FLOWS.one_of,
    cv.Optional(CONF_FLEX_ALIGN_MAIN, default="start"): flex_alignments,
    cv.Optional(
        CONF_FLEX_ALIGN_CROSS, default="start"
    ): LV_FLEX_CROSS_ALIGNMENTS.one_of,
    cv.Optional(CONF_FLEX_ALIGN_TRACK, default="start"): flex_alignments,
    cv.Optional(CONF_PAD_ROW): padding,
    cv.Optional(CONF_PAD_COLUMN): padding,
    cv.Optional(CONF_FLEX_GROW): cv.int_,
}

FLEX_HV_STYLE = {
    CONF_FLEX_ALIGN_MAIN: "LV_FLEX_ALIGN_SPACE_EVENLY",
    CONF_FLEX_ALIGN_TRACK: "LV_FLEX_ALIGN_CENTER",
    CONF_FLEX_ALIGN_CROSS: "LV_FLEX_ALIGN_CENTER",
    CONF_TYPE: TYPE_FLEX,
}

FLEX_OBJ_SCHEMA = {
    cv.Optional(CONF_FLEX_GROW): cv.int_,
}


def flex_hv_schema(dir):
    dir = CONF_HEIGHT if dir == "horizontal" else CONF_WIDTH
    return {
        cv.Optional(CONF_FLEX_GROW, default=1): cv.int_,
        cv.Optional(dir, default="100%"): size,
    }


def grid_free_space(value):
    value = cv.Upper(value)
    if value.startswith("FR(") and value.endswith(")"):
        value = value.removesuffix(")").removeprefix("FR(")
        return f"LV_GRID_FR({cv.positive_int(value)})"
    raise cv.Invalid("must be a size in pixels, CONTENT or FR(nn)")


grid_spec = cv.Any(size, LvConstant("LV_GRID_", "CONTENT").one_of, grid_free_space)


def grid_dimension(value):
    """
    Validator for a grid `rows` or `columns` value.
    Accepts either a positive integer (interpreted as that many cells of equal
    `LV_GRID_FR(1)` size) or a non-empty list of grid specs.
    """
    if isinstance(value, int):
        value = cv.int_range(min=1)(value)
        return ["LV_GRID_FR(1)"] * value
    result = cv.Schema([grid_spec])(value)
    if not result:
        raise cv.Invalid("Grid dimension list must contain at least one entry")
    return result


GRID_CELL_SCHEMA = {
    cv.Optional(CONF_GRID_CELL_ROW_POS): cv.positive_int,
    cv.Optional(CONF_GRID_CELL_COLUMN_POS): cv.positive_int,
    cv.Optional(CONF_GRID_CELL_ROW_SPAN): cv.int_range(min=1),
    cv.Optional(CONF_GRID_CELL_COLUMN_SPAN): cv.int_range(min=1),
    cv.Optional(CONF_GRID_CELL_X_ALIGN): grid_alignments,
    cv.Optional(CONF_GRID_CELL_Y_ALIGN): grid_alignments,
}


class Layout:
    """
    Define properties for a layout
    The base class is layout "none"
    """

    def get_type(self):
        return TYPE_NONE

    def get_layout_schemas(self, config: dict) -> tuple:
        """
        Get the layout and child schema for a given widget based on its layout type.
        """
        return None, {}

    def validate(self, config):
        """
        Validate the layout configuration. This is called late in the schema validation
        :param config: The input configuration
        :return: The validated configuration
        """
        return config


class FlexLayout(Layout):
    def get_type(self):
        return TYPE_FLEX

    def get_layout_schemas(self, config: dict) -> tuple:
        layout = config.get(CONF_LAYOUT)
        if not isinstance(layout, dict) or layout.get(CONF_TYPE).lower() != TYPE_FLEX:
            return None, {}
        child_schema = FLEX_OBJ_SCHEMA
        if grow := layout.get(CONF_FLEX_GROW):
            child_schema = {cv.Optional(CONF_FLEX_GROW, default=grow): cv.int_}
        # Polyfill to implement stretch alignment for flex containers
        # LVGL does not support this natively, so we add a 100% size property to the children in the cross-axis
        if layout.get(CONF_FLEX_ALIGN_CROSS) == "LV_FLEX_ALIGN_STRETCH":
            dimension = (
                CONF_WIDTH
                if "COLUMN" in layout[CONF_FLEX_FLOW].upper()
                else CONF_HEIGHT
            )
            child_schema[cv.Optional(dimension, default="100%")] = size
        return FLEX_LAYOUT_SCHEMA, child_schema

    def validate(self, config):
        """
        Perform validation on the container and its children for this layout
        :param config:
        :return:
        """
        return config


class DirectionalLayout(FlexLayout):
    def __init__(self, direction: str, flow):
        """
        :param direction: "horizontal" or "vertical"
        :param flow: "row" or "column"
        """
        super().__init__()
        self.direction = direction
        self.flow = flow

    def get_type(self):
        return self.direction

    def get_layout_schemas(self, config: dict) -> tuple:
        if not isinstance(config.get(CONF_LAYOUT), str):
            return None, {}
        if config.get(CONF_LAYOUT, "").lower() != self.direction:
            return None, {}
        return cv.one_of(self.direction, lower=True), flex_hv_schema(self.direction)

    def validate(self, config):
        assert config[CONF_LAYOUT].lower() == self.direction
        layout = {
            **FLEX_HV_STYLE,
            CONF_FLEX_FLOW: "LV_FLEX_FLOW_" + self.flow.upper(),
        }
        if pad_all := config.get("pad_all"):
            layout[CONF_PAD_ROW] = pad_all
            layout[CONF_PAD_COLUMN] = pad_all
        config[CONF_LAYOUT] = layout
        return config


class GridLayout(Layout):
    # Match shorthand grid layout strings: "NxM", "Nx" or "xM".
    # At least one of the two numbers must be present; this is enforced after matching.
    _GRID_LAYOUT_REGEX = re.compile(r"^\s*(\d+)?\s*x\s*(\d+)?\s*$")

    @staticmethod
    def _match_shorthand(layout):
        match = GridLayout._GRID_LAYOUT_REGEX.match(layout)
        if match is None or (match.group(1) is None and match.group(2) is None):
            return None
        return match

    def get_type(self):
        return TYPE_GRID

    def get_layout_schemas(self, config: dict) -> tuple:
        layout = config.get(CONF_LAYOUT)
        if isinstance(layout, str):
            if GridLayout._match_shorthand(layout):
                return (
                    cv.string,
                    {
                        cv.Optional(CONF_GRID_CELL_ROW_POS): cv.positive_int,
                        cv.Optional(CONF_GRID_CELL_COLUMN_POS): cv.positive_int,
                        cv.Optional(CONF_GRID_CELL_ROW_SPAN): cv.int_range(min=1),
                        cv.Optional(CONF_GRID_CELL_COLUMN_SPAN): cv.int_range(min=1),
                        cv.Optional(
                            CONF_GRID_CELL_X_ALIGN, default="center"
                        ): grid_alignments,
                        cv.Optional(
                            CONF_GRID_CELL_Y_ALIGN, default="center"
                        ): grid_alignments,
                    },
                )
            # Not a valid grid layout string
            return None, {}

        if not isinstance(layout, dict) or layout.get(CONF_TYPE).lower() != TYPE_GRID:
            return None, {}
        x_default = (
            "center" if isinstance(layout.get(CONF_GRID_ROWS), int) else cv.UNDEFINED
        )
        y_default = (
            "center" if isinstance(layout.get(CONF_GRID_COLUMNS), int) else cv.UNDEFINED
        )
        x_align = layout.get(CONF_GRID_CELL_X_ALIGN, x_default)
        y_align = layout.get(CONF_GRID_CELL_Y_ALIGN, y_default)
        return (
            {
                cv.Required(CONF_TYPE): cv.one_of(TYPE_GRID, lower=True),
                cv.Optional(CONF_GRID_ROWS): grid_dimension,
                cv.Optional(CONF_GRID_COLUMNS): grid_dimension,
                cv.Optional(CONF_GRID_COLUMN_ALIGN): grid_alignments,
                cv.Optional(CONF_GRID_ROW_ALIGN): grid_alignments,
                cv.Optional(CONF_PAD_ROW): padding,
                cv.Optional(CONF_PAD_COLUMN): padding,
                cv.Optional(CONF_MULTIPLE_WIDGETS_PER_CELL, default=False): cv.boolean,
                cv.Optional(CONF_GRID_CELL_X_ALIGN): grid_alignments,
                cv.Optional(CONF_GRID_CELL_Y_ALIGN): grid_alignments,
            },
            {
                cv.Optional(CONF_GRID_CELL_ROW_POS): cv.positive_int,
                cv.Optional(CONF_GRID_CELL_COLUMN_POS): cv.positive_int,
                cv.Optional(CONF_GRID_CELL_ROW_SPAN): cv.int_range(min=1),
                cv.Optional(CONF_GRID_CELL_COLUMN_SPAN): cv.int_range(min=1),
                cv.Optional(CONF_GRID_CELL_X_ALIGN, default=x_align): grid_alignments,
                cv.Optional(CONF_GRID_CELL_Y_ALIGN, default=y_align): grid_alignments,
            },
        )

    def validate(self, config: dict):
        """
        Validate the grid layout.
        The `layout:` key may be a dictionary with `rows` and/or `columns` keys, or a
        shorthand string in the format "<rows>x<columns>", "<rows>x" or "x<columns>".
        Either dimension may be omitted, in which case it will be calculated from the
        other dimension and the number of configured widgets.
        Either all cells must have a row and column,
        or none, in which case the grid layout is auto-generated.
        :param config:
        :return: The config updated with auto-generated values
        """
        layout = config.get(CONF_LAYOUT)
        widgets = config.get(CONF_WIDGETS, [])
        num_widgets = len(widgets)
        if isinstance(layout, str):
            # Shorthand string: "<rows>x<columns>", "<rows>x" or "x<columns>".
            # Each dimension defaults to LV_GRID_FR(1). A missing dimension is
            # calculated from the other dimension and the number of widgets.
            layout = layout.strip()
            match = GridLayout._match_shorthand(layout)
            if not match:
                raise cv.Invalid(
                    f"Invalid grid layout format: {layout!r}, expected "
                    "'<rows>x<columns>', '<rows>x' or 'x<columns>'",
                    [CONF_LAYOUT],
                )
            rows_int = int(match.group(1)) if match.group(1) is not None else None
            cols_int = int(match.group(2)) if match.group(2) is not None else None
            for label, val in (("row", rows_int), ("column", cols_int)):
                if val is not None and val < 1:
                    raise cv.Invalid(
                        f"Invalid grid layout {layout!r}: {label} count must be "
                        "at least 1",
                        [CONF_LAYOUT],
                    )
            if rows_int is not None and cols_int is not None:
                rows = rows_int
                cols = cols_int
            elif rows_int is not None:
                rows = rows_int
                cols = max(1, math.ceil(num_widgets / rows)) if num_widgets else 1
            else:
                cols = cols_int
                rows = max(1, math.ceil(num_widgets / cols)) if num_widgets else 1
            layout = {
                CONF_TYPE: TYPE_GRID,
                CONF_GRID_ROWS: ["LV_GRID_FR(1)"] * rows,
                CONF_GRID_COLUMNS: ["LV_GRID_FR(1)"] * cols,
            }
            config[CONF_LAYOUT] = layout
        # should be guaranteed to be a dict at this point
        assert isinstance(layout, dict)
        assert layout.get(CONF_TYPE).lower() == TYPE_GRID
        rows_list = layout.get(CONF_GRID_ROWS)
        cols_list = layout.get(CONF_GRID_COLUMNS)
        if rows_list is None and cols_list is None:
            raise cv.Invalid(
                "Grid layout requires at least one of 'rows' or 'columns' to be "
                "specified",
                [CONF_LAYOUT],
            )
        if rows_list is None:
            cols = len(cols_list)
            rows = max(1, math.ceil(num_widgets / cols)) if num_widgets else 1
            layout[CONF_GRID_ROWS] = ["LV_GRID_FR(1)"] * rows
        elif cols_list is None:
            rows = len(rows_list)
            cols = max(1, math.ceil(num_widgets / rows)) if num_widgets else 1
            layout[CONF_GRID_COLUMNS] = ["LV_GRID_FR(1)"] * cols
        allow_multiple = layout.get(CONF_MULTIPLE_WIDGETS_PER_CELL, False)
        rows = len(layout[CONF_GRID_ROWS])
        columns = len(layout[CONF_GRID_COLUMNS])
        used_cells = [[None] * columns for _ in range(rows)]
        for index, widget in enumerate(config.get(CONF_WIDGETS, [])):
            _, w = next(iter(widget.items()))
            if (CONF_GRID_CELL_COLUMN_POS in w) != (CONF_GRID_CELL_ROW_POS in w):
                raise cv.Invalid(
                    "Both row and column positions must be specified, or both omitted",
                    [CONF_WIDGETS, index],
                )
            if CONF_GRID_CELL_ROW_POS in w:
                row = w[CONF_GRID_CELL_ROW_POS]
                column = w[CONF_GRID_CELL_COLUMN_POS]
            else:
                try:
                    row, column = next(
                        (r_idx, c_idx)
                        for r_idx, row in enumerate(used_cells)
                        for c_idx, value in enumerate(row)
                        if value is None
                    )
                except StopIteration:
                    raise cv.Invalid(
                        "No free cells available in grid layout", [CONF_WIDGETS, index]
                    ) from None
                w[CONF_GRID_CELL_ROW_POS] = row
                w[CONF_GRID_CELL_COLUMN_POS] = column

            row_span = w.get(CONF_GRID_CELL_ROW_SPAN, 1)
            column_span = w.get(CONF_GRID_CELL_COLUMN_SPAN, 1)
            for i in range(row_span):
                for j in range(column_span):
                    if row + i >= rows or column + j >= columns:
                        raise cv.Invalid(
                            f"Cell at {row}/{column} span {row_span}x{column_span} "
                            f"exceeds grid size {rows}x{columns}",
                            [CONF_WIDGETS, index],
                        )
                    if (
                        not allow_multiple
                        and used_cells[row + i][column + j] is not None
                    ):
                        raise cv.Invalid(
                            f"Cell span {row + i}/{column + j} already occupied by widget at index {used_cells[row + i][column + j]}",
                            [CONF_WIDGETS, index],
                        )
                    used_cells[row + i][column + j] = index

        return config


LAYOUT_CLASSES = (
    FlexLayout(),
    GridLayout(),
    DirectionalLayout("horizontal", "row"),
    DirectionalLayout("vertical", "column"),
)
LAYOUT_CHOICES = [x.get_type() for x in LAYOUT_CLASSES]


def append_layout_schema(schema, config: dict):
    """
    Get the child layout schema for a given widget based on its layout type.
    :param config: The config to check
    :return: A schema for the layout including a widgets key
    """
    # Local import to avoid circular dependencies
    if CONF_WIDGETS not in config:
        if CONF_LAYOUT in config:
            raise cv.Invalid(
                f"Layout {config[CONF_LAYOUT]} requires a {CONF_WIDGETS} key",
                [CONF_LAYOUT],
            )
        return schema

    from .schemas import any_widget_schema

    if CONF_LAYOUT not in config:
        # If no layout is specified, return the schema as is
        return schema.extend({cv.Optional(CONF_WIDGETS): any_widget_schema()})
    layout = config[CONF_LAYOUT]
    # Sanity check the layout to avoid redundant checks in each type
    if not isinstance(layout, str) and not isinstance(layout, dict):
        raise cv.Invalid(
            "The 'layout' option must be a string or a dictionary", [CONF_LAYOUT]
        )
    if isinstance(layout, dict) and not isinstance(layout.get(CONF_TYPE), str):
        raise cv.Invalid(
            "Invalid layout type; must be a string ('flex' or 'grid')",
            [CONF_LAYOUT, CONF_TYPE],
        )

    for layout_class in LAYOUT_CLASSES:
        layout_schema, child_schema = layout_class.get_layout_schemas(config)
        if layout_schema:
            layout_schema = cv.Schema(
                {
                    cv.Required(CONF_LAYOUT): layout_schema,
                    cv.Required(CONF_WIDGETS): any_widget_schema(child_schema),
                }
            )
            layout_schema.add_extra(layout_class.validate)
            return layout_schema.extend(schema)

    if isinstance(layout, dict):
        raise cv.Invalid(
            "Invalid layout type; must be 'flex' or 'grid'", [CONF_LAYOUT, CONF_TYPE]
        )
    raise cv.Invalid(
        textwrap.dedent(
            """
                Invalid 'layout' value
                layout choices are 'horizontal', 'vertical',
                '<rows>x<cols>', '<rows>x', 'x<cols>',
                or a dictionary with a 'type' key
            """
        ),
        [CONF_LAYOUT],
    )
