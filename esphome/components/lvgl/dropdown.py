import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS

from .codegen import set_obj_properties
from .defines import (
    CONF_DIR,
    CONF_DROPDOWN,
    CONF_DROPDOWN_LIST,
    CONF_LABEL,
    CONF_SELECTED_INDEX,
    CONF_SYMBOL,
    DIRECTIONS,
)
from .lv_validation import lv_int, lv_text, option_string
from .schemas import part_schema
from .types import lv_dropdown_list_t, lv_dropdown_t
from .widget import Widget, WidgetType, add_temp_var

DROPDOWN_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SYMBOL): lv_text,
        cv.Optional(CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(CONF_DIR, default="BOTTOM"): DIRECTIONS.one_of,
        cv.Optional(CONF_DROPDOWN_LIST): part_schema(CONF_DROPDOWN_LIST),
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
            DROPDOWN_SCHEMA,
            DROPDOWN_BASE_SCHEMA,
        )

    @property
    def w_type(self):
        return lv_dropdown_t

    async def to_code(self, w: Widget, config):
        obj = w.obj
        init = []
        if options := config.get(CONF_OPTIONS):
            text = cg.safe_exp("\n".join(options))
            init.extend(w.set_property("options", text))
        if symbol := config.get(CONF_SYMBOL):
            init.extend(w.set_property("symbol", await lv_text.process(symbol)))
        if selected := config.get(CONF_SELECTED_INDEX):
            value = await lv_int.process(selected)
            init.extend(w.set_property("selected", value))
        if dirn := config.get(CONF_DIR):
            init.extend(w.set_property("dir", dirn))
        if dlist := config.get(CONF_DROPDOWN_LIST):
            s = Widget(w, lv_dropdown_list_t, dlist, f"{w.obj}__list")
            init.extend(add_temp_var("lv_obj_t", s.obj))
            init.append(f"{s.obj} = lv_dropdown_get_list({obj});")
            init.extend(await set_obj_properties(s, dlist))
        return init

    def get_uses(self):
        return (CONF_LABEL,)


dropdown_spec = DropdownType()
