import esphome.config_validation as cv
from esphome.const import CONF_MODE
from .defines import CONF_KEYBOARD, KEYBOARD_MODES, CONF_TEXTAREA
from .types import lv_textarea_t, lv_keyboard_t
from .widget import get_widget, Widget, WidgetType

KEYBOARD_SCHEMA = {
    cv.Optional(CONF_MODE, default="TEXT_UPPER"): KEYBOARD_MODES.one_of,
    cv.Optional(CONF_TEXTAREA): cv.use_id(lv_textarea_t),
}


class KeyboardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_KEYBOARD,
            KEYBOARD_SCHEMA,
        )

    @property
    def w_type(self):
        return lv_keyboard_t

    async def to_code(self, w: Widget, config: dict):
        init = []
        init.extend(w.set_property(CONF_MODE, config))
        if ta := config.get(CONF_TEXTAREA):
            ta = await get_widget(ta)
            init.extend(w.set_property(CONF_TEXTAREA, ta.obj))
        return init


keyboard_spec = KeyboardType()
