import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS

from ..defines import (
    CONF_DIR,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_SCROLLBAR,
    CONF_SELECTED,
    CONF_SELECTED_INDEX,
    CONF_SYMBOL,
    DIRECTIONS,
    literal,
)
from ..lv_validation import lv_int, lv_text, option_string
from ..lvcode import LocalVariable, lv, lv_expr
from ..schemas import part_schema
from ..types import LvSelect, LvType, lv_obj_t
from . import Widget, WidgetType, set_obj_properties
from .label import CONF_LABEL

CONF_DROPDOWN = "dropdown"
CONF_DROPDOWN_LIST = "dropdown_list"

lv_dropdown_t = LvSelect("lv_dropdown_t")
lv_dropdown_list_t = LvType("lv_dropdown_list_t")
dropdown_list_spec = WidgetType(
    CONF_DROPDOWN_LIST, lv_dropdown_list_t, (CONF_MAIN, CONF_SELECTED, CONF_SCROLLBAR)
)

DROPDOWN_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SYMBOL): lv_text,
        cv.Optional(CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(CONF_DIR, default="BOTTOM"): DIRECTIONS.one_of,
        cv.Optional(CONF_DROPDOWN_LIST): part_schema(dropdown_list_spec),
    }
)

DROPDOWN_SCHEMA = DROPDOWN_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(option_string),
    }
)


class DropdownType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_DROPDOWN,
            lv_dropdown_t,
            (CONF_MAIN, CONF_INDICATOR),
            DROPDOWN_SCHEMA,
            DROPDOWN_BASE_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        if options := config.get(CONF_OPTIONS):
            text = cg.safe_exp("\n".join(options))
            lv.dropdown_set_options(w.obj, text)
        if symbol := config.get(CONF_SYMBOL):
            lv.dropdown_set_symbol(w.obj, await lv_text.process(symbol))
        if (selected := config.get(CONF_SELECTED_INDEX)) is not None:
            value = await lv_int.process(selected)
            lv.dropdown_set_selected(w.obj, value)
        if dirn := config.get(CONF_DIR):
            lv.dropdown_set_dir(w.obj, literal(dirn))
        if dlist := config.get(CONF_DROPDOWN_LIST):
            with LocalVariable(
                "dropdown_list", lv_obj_t, lv_expr.dropdown_get_list(w.obj)
            ) as dlist_obj:
                dwid = Widget(dlist_obj, dropdown_list_spec, dlist)
                await set_obj_properties(dwid, dlist)

    def get_uses(self):
        return (CONF_LABEL,)


dropdown_spec = DropdownType()
