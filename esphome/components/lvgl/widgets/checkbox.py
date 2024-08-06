from esphome.const import CONF_TEXT

from ..defines import CONF_INDICATOR, CONF_MAIN
from ..lv_validation import lv_text
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
            TEXT_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        if (value := config.get(CONF_TEXT)) is not None:
            lv.checkbox_set_text(w.obj, await lv_text.process(value))


checkbox_spec = CheckboxType()
