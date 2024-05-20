from .defines import CONF_CHECKBOX, CONF_TEXT
from .lv_validation import lv_text
from .schemas import TEXT_SCHEMA
from .types import lv_checkbox_t
from .widget import Widget, WidgetType


class CheckboxType(WidgetType):
    def __init__(self):
        super().__init__(CONF_CHECKBOX, TEXT_SCHEMA)

    @property
    def w_type(self):
        return lv_checkbox_t

    async def to_code(self, w: Widget, config):
        """For a text object, create and set text"""
        if value := config.get(CONF_TEXT):
            return w.set_property("text", await lv_text.process(value))
        return []


checkbox_spec = CheckboxType()
