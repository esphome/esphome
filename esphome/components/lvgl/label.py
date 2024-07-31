import esphome.config_validation as cv

from .defines import (
    CONF_LABEL,
    CONF_LONG_MODE,
    CONF_MAIN,
    CONF_RECOLOR,
    CONF_SCROLLBAR,
    CONF_SELECTED,
    CONF_TEXT,
    LV_LONG_MODES,
)
from .lv_validation import lv_bool, lv_text
from .schemas import TEXT_SCHEMA
from .types import LvText, WidgetType
from .widget import Widget


class LabelType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_LABEL,
            LvText("lv_label_t"),
            (CONF_MAIN, CONF_SCROLLBAR, CONF_SELECTED),
            TEXT_SCHEMA.extend(
                {
                    cv.Optional(CONF_RECOLOR): lv_bool,
                    cv.Optional(CONF_LONG_MODE): LV_LONG_MODES.one_of,
                }
            ),
        )

    async def to_code(self, w: Widget, config):
        """For a text object, create and set text"""
        if value := config.get(CONF_TEXT):
            w.set_property(CONF_TEXT, await lv_text.process(value))
        w.set_property(CONF_LONG_MODE, config)
        w.set_property(CONF_RECOLOR, config)


label_spec = LabelType()
