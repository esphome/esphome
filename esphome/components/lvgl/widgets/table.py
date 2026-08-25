from contextlib import ExitStack

from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ITEMS, CONF_ROW, CONF_TEXT, CONF_WIDTH
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.types import ConfigFragmentType, ConfigType, SafeExpType

from ..automation import action_to_code
from ..defines import CONF_COLUMN, CONF_MAIN, LValidator, literal
from ..lv_validation import lv_int, lv_text, pixels_or_percent, pixels_validator
from ..lvcode import LocalVariable, lv, lv_add, lv_expr
from ..types import LvCompound, LvType, ObjUpdateAction, lv_coord_t
from . import Widget, WidgetType, get_widgets
from .label import CONF_LABEL

CONF_TABLE = "table"
CONF_CELLS = "cells"
CONF_COLUMNS = "columns"
CONF_ROW_COUNT = "row_count"
CONF_COLUMN_COUNT = "column_count"
CONF_MERGE_RIGHT = "merge_right"
CONF_TEXT_CROP = "text_crop"
CONF_SELECTED_ROW = "selected_row"
CONF_SELECTED_COLUMN = "selected_column"

CELL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEXT, default=""): lv_text,
        # Not templatable: the value selects between two different LVGL calls
        # (set/clear cell ctrl), so a runtime lambda can't be mapped to a single call.
        cv.Optional(CONF_MERGE_RIGHT): cv.boolean,
        cv.Optional(CONF_TEXT_CROP): cv.boolean,
    }
)

# A cell can be given as a bare piece of text, or a dict for more control
TABLE_CELL_SCHEMA = cv.maybe_simple_value(CELL_SCHEMA, key=CONF_TEXT)

# A row can be given as a bare list of cells, or a dict for future extension
ROW_SCHEMA = cv.maybe_simple_value(
    cv.Schema({cv.Required(CONF_CELLS): cv.ensure_list(TABLE_CELL_SCHEMA)}),
    key=CONF_CELLS,
)


def _column_width_validator(value: ConfigFragmentType) -> int | float | list[str]:
    """Like pixels_or_percent, but rejects negative widths, which would
    defeat the 100%-total check and wrap around in the generated uint8_t pct."""
    if value == SCHEMA_EXTRACT:
        return ["pixels", "..%"]
    return cv.Any(pixels_validator, cv.percentage)(value)


column_width = LValidator(
    _column_width_validator,
    lv_coord_t,
    retmapper=pixels_or_percent.retmapper,
    animatable=True,
)

COLUMN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH): column_width,
    }
)


def _validate_table(config: ConfigType) -> ConfigType:
    rows = config.get(CONF_ROWS)
    min_row_count = len(rows) if rows else 0
    min_column_count = max(len(row[CONF_CELLS]) for row in rows) if rows else 0
    row_count = config.get(CONF_ROW_COUNT)
    if row_count is not None and row_count < min_row_count:
        raise cv.Invalid(
            f"{CONF_ROW_COUNT} must be at least {min_row_count} to hold all the given rows",
            path=[CONF_ROW_COUNT],
        )
    column_count = config.get(CONF_COLUMN_COUNT)
    if column_count is not None and column_count < min_column_count:
        raise cv.Invalid(
            f"{CONF_COLUMN_COUNT} must be at least {min_column_count} to hold all the cells in a row",
            path=[CONF_COLUMN_COUNT],
        )
    column_count = column_count if column_count is not None else min_column_count
    columns = config.get(CONF_COLUMNS)
    if columns and column_count and len(columns) > column_count:
        raise cv.Invalid(
            f"{CONF_COLUMNS} defines {len(columns)} columns, but the table has only {column_count}",
            path=[CONF_COLUMNS],
        )
    total_pct = sum(
        width
        for column in columns or ()
        if isinstance((width := column.get(CONF_WIDTH)), float)
    )
    if total_pct > 1.0:
        raise cv.Invalid(
            f"{CONF_COLUMNS} percentage widths add up to {total_pct * 100:.0f}%, which exceeds 100%",
            path=[CONF_COLUMNS],
        )
    return config


TABLE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ROWS): cv.ensure_list(ROW_SCHEMA),
        cv.Optional(CONF_ROW_COUNT): cv.positive_int,
        cv.Optional(CONF_COLUMN_COUNT): cv.positive_int,
        cv.Optional(CONF_COLUMNS): cv.ensure_list(COLUMN_SCHEMA),
        cv.Optional(CONF_SELECTED_ROW): lv_int,
        cv.Optional(CONF_SELECTED_COLUMN): lv_int,
    }
).add_extra(_validate_table)

lv_table_t = LvType(
    "LvTableType",
    parents=(LvCompound,),
    largs=[(cg.uint32, "row"), (cg.uint32, "column")],
    lvalue=lambda w: [
        lv_expr.table_get_selected_row(w.obj),
        lv_expr.table_get_selected_column(w.obj),
    ],
    has_on_value=True,
)


async def set_cell_ctrl(
    w: Widget, row: SafeExpType, column: SafeExpType, cell: ConfigType
) -> None:
    for key, ctrl in (
        (CONF_MERGE_RIGHT, "LV_TABLE_CELL_CTRL_MERGE_RIGHT"),
        (CONF_TEXT_CROP, "LV_TABLE_CELL_CTRL_TEXT_CROP"),
    ):
        if key not in cell:
            continue
        if cell[key]:
            lv.table_set_cell_ctrl(w.obj, row, column, literal(ctrl))
        else:
            lv.table_clear_cell_ctrl(w.obj, row, column, literal(ctrl))


async def set_selected_cell(w: Widget, config: ConfigType) -> None:
    selected_row = config.get(CONF_SELECTED_ROW)
    selected_column = config.get(CONF_SELECTED_COLUMN)
    if selected_row is None and selected_column is None:
        return
    # LV_TABLE_CELL_NONE selects the whole column/row when only one index is given
    row_value = (
        await lv_int.process(selected_row)
        if selected_row is not None
        else literal("LV_TABLE_CELL_NONE")
    )
    column_value = (
        await lv_int.process(selected_column)
        if selected_column is not None
        else literal("LV_TABLE_CELL_NONE")
    )
    lv.table_set_selected_cell(w.obj, row_value, column_value)


TABLE_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SELECTED_ROW): lv_int,
        cv.Optional(CONF_SELECTED_COLUMN): lv_int,
    }
)


class TableType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_TABLE,
            lv_table_t,
            (CONF_MAIN, CONF_ITEMS),
            TABLE_SCHEMA,
            modify_schema=TABLE_MODIFY_SCHEMA,
        )

    def get_uses(self) -> tuple[str]:
        return (CONF_LABEL,)

    async def to_code(self, w: Widget, config: dict) -> None:
        rows = config.get(CONF_ROWS)
        row_count = config.get(CONF_ROW_COUNT)
        column_count = config.get(CONF_COLUMN_COUNT)
        if rows is not None:
            if row_count is None:
                row_count = len(rows)
            if column_count is None:
                column_count = max((len(row[CONF_CELLS]) for row in rows), default=0)
        if row_count is not None:
            lv.table_set_row_count(w.obj, row_count)
        if column_count is not None:
            lv.table_set_column_count(w.obj, column_count)
        columns = config.get(CONF_COLUMNS, ())
        pct_column_count = sum(
            1 for column in columns if isinstance(column.get(CONF_WIDTH), float)
        )
        if pct_column_count:
            lv_add(w.var.init_column_pct(pct_column_count))
        for index, column in enumerate(columns):
            if (width := column.get(CONF_WIDTH)) is None:
                continue
            if isinstance(width, float):
                # A percentage: column_width validation leaves it as a 0.0-1.0
                # fraction. LVGL's table widget only accepts a literal pixel width, so
                # the actual width is recomputed at runtime from the table's own size.
                lv_add(w.var.add_column_width_pct(index, round(width * 100)))
            else:
                lv.table_set_column_width(
                    w.obj, index, await column_width.process(width)
                )
        for row_index, row in enumerate(rows or ()):
            for column_index, cell in enumerate(row[CONF_CELLS]):
                lv.table_set_cell_value(
                    w.obj,
                    row_index,
                    column_index,
                    await lv_text.process(cell[CONF_TEXT]),
                )
                await set_cell_ctrl(w, row_index, column_index, cell)
        await set_selected_cell(w, config)


table_spec = TableType()


@automation.register_action(
    "lvgl.table.cell.update",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_table_t),
            cv.Required(CONF_ROW): lv_int,
            cv.Required(CONF_COLUMN): lv_int,
            cv.Optional(CONF_TEXT): lv_text,
            cv.Optional(CONF_MERGE_RIGHT): cv.boolean,
            cv.Optional(CONF_TEXT_CROP): cv.boolean,
        }
    ).add_extra(cv.has_at_least_one_key(CONF_TEXT, CONF_MERGE_RIGHT, CONF_TEXT_CROP)),
    synchronous=True,
)
async def table_cell_update_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    widgets = await get_widgets(config)

    async def do_update(w: Widget):
        row = await lv_int.process(config[CONF_ROW])
        column = await lv_int.process(config[CONF_COLUMN])
        fields_set = sum(
            key in config for key in (CONF_TEXT, CONF_MERGE_RIGHT, CONF_TEXT_CROP)
        )
        with ExitStack() as stack:
            if fields_set > 1:
                # row/column feed more than one generated call below: cache them in
                # local variables so a !lambda value is only evaluated once.
                row = stack.enter_context(
                    LocalVariable("row", cg.int_, row, modifier="")
                )
                column = stack.enter_context(
                    LocalVariable("column", cg.int_, column, modifier="")
                )
            if CONF_TEXT in config:
                lv.table_set_cell_value(
                    w.obj, row, column, await lv_text.process(config[CONF_TEXT])
                )
            await set_cell_ctrl(w, row, column, config)

    return await action_to_code(
        widgets, do_update, action_id, template_arg, args, config
    )
