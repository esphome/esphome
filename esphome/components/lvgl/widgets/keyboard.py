from esphome.components.key_provider import KeyProvider
import esphome.config_validation as cv
from esphome.const import CONF_ITEMS, CONF_MODE
from esphome.cpp_types import std_string

from ..defines import CONF_MAIN, KEYBOARD_MODES, add_lv_use, literal
from ..types import LvCompound, LvType
from . import Widget, WidgetType, get_widgets
from .buttonmatrix import CONF_BUTTONMATRIX
from .label import CONF_LABEL
from .textarea import CONF_TEXTAREA, lv_textarea_t

CONF_KEYBOARD = "keyboard"

KEYBOARD_SCHEMA = {
    cv.Optional(CONF_MODE, default="TEXT_UPPER"): KEYBOARD_MODES.one_of,
    cv.Optional(CONF_TEXTAREA): cv.use_id(lv_textarea_t),
}

KEYBOARD_MODIFY_SCHEMA = {
    cv.Optional(CONF_MODE): KEYBOARD_MODES.one_of,
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
            modify_schema=KEYBOARD_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return CONF_KEYBOARD, CONF_TEXTAREA, CONF_BUTTONMATRIX, CONF_LABEL

    async def to_code(self, w: Widget, config: dict):
        add_lv_use("KEY_LISTENER")
        if mode := config.get(CONF_MODE):
            await w.set_property(CONF_MODE, await KEYBOARD_MODES.process(mode))
        if config.get(CONF_TEXTAREA):
            # Handles updates in automations, and initial config.
            await w.set_property(
                CONF_TEXTAREA, (await get_widgets(config, CONF_TEXTAREA))[0].obj
            )


keyboard_spec = KeyboardType()
