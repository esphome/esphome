from esphome import codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS

from ..defines import (
    CONF_DIR,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_SCROLLBAR,
    CONF_SELECTED,
    CONF_SELECTED_INDEX,
    CONF_SELECTED_TEXT,
    CONF_SYMBOL,
    DIRECTIONS,
    literal,
)
from ..helpers import lvgl_components_required
from ..lv_validation import lv_int, lv_text, option_string
from ..lvcode import LocalVariable, lv, lv_add, lv_expr
from ..schemas import part_schema
from ..types import LvCompound, LvSelect, LvType, lv_obj_t
from . import Widget, WidgetType, set_obj_properties
from .label import CONF_LABEL

CONF_DROPDOWN = "dropdown"
CONF_DROPDOWN_LIST = "dropdown_list"

# Example valid dropdown symbol (left arrow) for error messages
EXAMPLE_DROPDOWN_SYMBOL = "\U00002190"  # ←


def dropdown_symbol_validator(value):
    """
    Validate that the dropdown symbol is a single Unicode character
    with a codepoint of 0x100 (256) or greater.
    This is required because LVGL uses codepoints below 0x100 for internal symbols.
    """
    value = cv.string(value)
    # len(value) counts Unicode code points, not grapheme clusters or bytes
    if len(value) != 1:
        raise cv.Invalid(
            f"Dropdown symbol must be a single character, got '{value}' with length {len(value)}"
        )
    codepoint = ord(value)
    if codepoint < 0x100:
        # Format the example symbol as a Unicode escape for the error message
        example_escape = f"\\U{ord(EXAMPLE_DROPDOWN_SYMBOL):08X}"
        raise cv.Invalid(
            f"Dropdown symbol must have a Unicode codepoint of 0x100 (256) or greater. "
            f"'{value}' has codepoint {codepoint} (0x{codepoint:X}). "
            f"Use a character like '{example_escape}' ({EXAMPLE_DROPDOWN_SYMBOL}) or other Unicode symbols with codepoint >= 0x100."
        )
    return value


lv_dropdown_t = LvSelect("LvDropdownType", parents=(LvCompound,))

lv_dropdown_list_t = LvType("lv_dropdown_list_t")
dropdown_list_spec = WidgetType(
    CONF_DROPDOWN_LIST, lv_dropdown_list_t, (CONF_MAIN, CONF_SELECTED, CONF_SCROLLBAR)
)

DROPDOWN_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SYMBOL): dropdown_symbol_validator,
        cv.Exclusive(CONF_SELECTED_INDEX, CONF_SELECTED_TEXT): lv_int,
        cv.Exclusive(CONF_SELECTED_TEXT, CONF_SELECTED_TEXT): lv_text,
        cv.Optional(CONF_DROPDOWN_LIST): part_schema(dropdown_list_spec.parts),
    }
)

DROPDOWN_SCHEMA = DROPDOWN_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(option_string),
        cv.Optional(CONF_DIR, default="BOTTOM"): DIRECTIONS.one_of,
    }
)

DROPDOWN_UPDATE_SCHEMA = DROPDOWN_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_OPTIONS): cv.ensure_list(option_string),
        cv.Optional(CONF_DIR): DIRECTIONS.one_of,
    }
)


class DropdownType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_DROPDOWN,
            lv_dropdown_t,
            (CONF_MAIN, CONF_INDICATOR),
            DROPDOWN_SCHEMA,
            modify_schema=DROPDOWN_UPDATE_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        lvgl_components_required.add(CONF_DROPDOWN)
        if options := config.get(CONF_OPTIONS):
            lv_add(w.var.set_options(options))
        if symbol := config.get(CONF_SYMBOL):
            lv.dropdown_set_symbol(w.var.obj, cg.safe_exp(symbol))
        if (selected := config.get(CONF_SELECTED_INDEX)) is not None:
            value = await lv_int.process(selected)
            lv_add(w.var.set_selected_index(value, literal("LV_ANIM_OFF")))
        if (selected := config.get(CONF_SELECTED_TEXT)) is not None:
            value = await lv_text.process(selected)
            lv_add(w.var.set_selected_text(value, literal("LV_ANIM_OFF")))
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
