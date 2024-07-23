import esphome.config_validation as cv

from .codegen import widget_to_code
from .defines import (
    CONF_BTN,
    CONF_BTNMATRIX,
    CONF_ENTRIES,
    CONF_HEADER_MODE,
    CONF_IMG,
    CONF_LABEL,
    CONF_MENU,
    CONF_ROOT_BACK_BTN,
    LV_MENU_MODES,
    LvConstant,
)
from .schemas import container_schema
from .types import lv_menu_t
from .widget import Widget, WidgetType

MENU_SCHEMA = {
    cv.Optional(CONF_HEADER_MODE, default="top_fixed"): LV_MENU_MODES.one_of,
    cv.Optional(CONF_ROOT_BACK_BTN, default="disabled"): LvConstant(
        "LV_MENU_ROOT_BACK_BTN_", "DISABLED", "ENABLED"
    ).one_of,
    cv.Optional(CONF_ENTRIES): cv.ensure_list(container_schema(CONF_MENU)),
}


class MenuType(WidgetType):
    def __init__(self):
        super().__init__(CONF_MENU, MENU_SCHEMA)

    @property
    def w_type(self):
        return lv_menu_t

    async def to_code(self, w: Widget, config: dict):
        init = []
        init.extend(w.set_property("mode_header", config[CONF_HEADER_MODE]))
        init.extend(w.set_property("mode_root_back_btn", config[CONF_ROOT_BACK_BTN]))
        if entries := config.get(CONF_ENTRIES):
            for econf in entries:
                # id = econf[CONF_ID]
                init.extend(await widget_to_code(econf, "menu_entry", w))
        return init

    def get_uses(self):
        return (CONF_BTNMATRIX, CONF_IMG, CONF_LABEL, CONF_BTN)


menu_spec = MenuType()


class MenuEntryType(WidgetType):
    def obj_creator(self, parent: Widget, config: dict):
        return f"lv_menu_cont_create({parent.obj})"

    async def to_code(self, w: Widget, config: dict):
        return []
