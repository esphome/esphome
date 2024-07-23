import esphome.config_validation as cv
from esphome.const import CONF_MODE

from .defines import CONF_KEYBOARD, CONF_TEXTAREA, KEYBOARD_MODES
from .helpers import add_lv_use
from .types import lv_keyboard_t, lv_textarea_t
from .widget import Widget, WidgetType, get_widget

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
        add_lv_use("textarea")
        add_lv_use("btnmatrix")
        init.extend(w.set_property(CONF_MODE, config))
        if ta := config.get(CONF_TEXTAREA):
            ta = await get_widget(ta)
            init.extend(w.set_property(CONF_TEXTAREA, ta.obj))
        return init


keyboard_spec = KeyboardType()
