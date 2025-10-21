import esphome.config_validation as cv
from esphome.const import CONF_MODE, CONF_OPTIONS

from ..defines import (
    CONF_ANIMATED,
    CONF_MAIN,
    CONF_SELECTED,
    CONF_SELECTED_INDEX,
    CONF_SELECTED_TEXT,
    CONF_VISIBLE_ROW_COUNT,
    ROLLER_MODES,
    literal,
)
from ..helpers import lvgl_components_required
from ..lv_validation import animated, lv_int, lv_text, option_string
from ..lvcode import lv_add
from ..types import LvSelect
from . import WidgetType
from .label import CONF_LABEL

CONF_ROLLER = "roller"
lv_roller_t = LvSelect("LvRollerType")

ROLLER_BASE_SCHEMA = cv.Schema(
    {
        cv.Exclusive(CONF_SELECTED_INDEX, CONF_SELECTED_TEXT): lv_int,
        cv.Exclusive(CONF_SELECTED_TEXT, CONF_SELECTED_TEXT): lv_text,
        cv.Optional(CONF_VISIBLE_ROW_COUNT): lv_int,
        cv.Optional(CONF_MODE): ROLLER_MODES.one_of,
    }
)

ROLLER_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(option_string),
    }
)

ROLLER_MODIFY_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATED, default=True): animated,
        cv.Optional(CONF_OPTIONS): cv.ensure_list(option_string),
    }
)


class RollerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ROLLER,
            lv_roller_t,
            (CONF_MAIN, CONF_SELECTED),
            ROLLER_SCHEMA,
            ROLLER_MODIFY_SCHEMA,
        )

    async def to_code(self, w, config):
        lvgl_components_required.add(CONF_ROLLER)
        if mode := config.get(CONF_MODE):
            mode = await ROLLER_MODES.process(mode)
            lv_add(w.var.set_mode(mode))
        if options := config.get(CONF_OPTIONS):
            lv_add(w.var.set_options(options))
        animopt = literal(config.get(CONF_ANIMATED, "LV_ANIM_OFF"))
        if (selected := config.get(CONF_SELECTED_INDEX)) is not None:
            value = await lv_int.process(selected)
            lv_add(w.var.set_selected_index(value, animopt))
        if (selected := config.get(CONF_SELECTED_TEXT)) is not None:
            value = await lv_text.process(selected)
            lv_add(w.var.set_selected_text(value, animopt))
        await w.set_property(
            CONF_VISIBLE_ROW_COUNT,
            await lv_int.process(config.get(CONF_VISIBLE_ROW_COUNT)),
        )

    @property
    def animated(self):
        return True

    def get_uses(self):
        return (CONF_LABEL,)


roller_spec = RollerType()
