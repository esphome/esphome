from esphome.components.key_provider import KeyProvider
import esphome.config_validation as cv
from esphome.const import CONF_ITEMS, CONF_MODE
from esphome.cpp_types import std_string

from ..defines import CONF_MAIN, KEYBOARD_MODES, literal
from ..helpers import add_lv_use, lvgl_components_required
from ..types import LvCompound, LvType
from . import Widget, WidgetType, get_widgets
from .textarea import CONF_TEXTAREA, lv_textarea_t

CONF_KEYBOARD = "keyboard"

KEYBOARD_SCHEMA = {
    cv.Optional(CONF_MODE, default="TEXT_UPPER"): KEYBOARD_MODES.one_of,
    cv.Optional(CONF_TEXTAREA): cv.use_id(lv_textarea_t),
}

lv_keyboard_t = LvType(
    "LvKeyboardType",
    parents=(KeyProvider, LvCompound),
    largs=[(std_string, "text")],
    has_on_value=True,
    lvalue=lambda w: literal(f"lv_textarea_get_text({w.obj})"),
)


class KeyboardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_KEYBOARD,
            lv_keyboard_t,
            (CONF_MAIN, CONF_ITEMS),
            KEYBOARD_SCHEMA,
        )

    def get_uses(self):
        return CONF_KEYBOARD, CONF_TEXTAREA

    async def to_code(self, w: Widget, config: dict):
        lvgl_components_required.add("KEY_LISTENER")
        lvgl_components_required.add(CONF_KEYBOARD)
        add_lv_use("btnmatrix")
        await w.set_property(CONF_MODE, await KEYBOARD_MODES.process(config[CONF_MODE]))
        if ta := await get_widgets(config, CONF_TEXTAREA):
            await w.set_property(CONF_TEXTAREA, ta[0].obj)


keyboard_spec = KeyboardType()
