from esphome.config_validation import Optional
from esphome.const import CONF_TEXT

from ..defines import CONF_INDICATOR, CONF_MAIN, CONF_PAD_COLUMN
from ..lv_validation import lv_text, padding
from ..lvcode import lv
from ..schemas import TEXT_SCHEMA
from ..types import LvBoolean
from . import Widget, WidgetType

CONF_CHECKBOX = "checkbox"


class CheckboxType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_CHECKBOX,
            LvBoolean("lv_checkbox_t"),
            (CONF_MAIN, CONF_INDICATOR),
            TEXT_SCHEMA.extend(
                {
                    Optional(CONF_PAD_COLUMN): padding,
                }
            ),
        )

    async def to_code(self, w: Widget, config):
        if (value := config.get(CONF_TEXT)) is not None:
            lv.checkbox_set_text(w.obj, await lv_text.process(value))


checkbox_spec = CheckboxType()
