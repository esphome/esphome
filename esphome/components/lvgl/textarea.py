import esphome.config_validation as cv
from esphome.const import CONF_MAX_LENGTH

from .defines import (
    CONF_ACCEPTED_CHARS,
    CONF_ONE_LINE,
    CONF_PASSWORD_MODE,
    CONF_PLACEHOLDER_TEXT,
    CONF_TEXT,
    CONF_TEXTAREA,
)
from .lv_validation import lv_bool, lv_int, lv_text
from .schemas import TEXT_SCHEMA
from .types import lv_textarea_t
from .widget import Widget, WidgetType

TEXTAREA_SCHEMA = TEXT_SCHEMA.extend(
    {
        cv.Optional(CONF_PLACEHOLDER_TEXT): lv_text,
        cv.Optional(CONF_ACCEPTED_CHARS): lv_text,
        cv.Optional(CONF_ONE_LINE): lv_bool,
        cv.Optional(CONF_PASSWORD_MODE): lv_bool,
        cv.Optional(CONF_MAX_LENGTH): lv_int,
    }
)


class TextareaType(WidgetType):
    def __init__(self):
        super().__init__(CONF_TEXTAREA, TEXTAREA_SCHEMA)

    @property
    def w_type(self):
        return lv_textarea_t

    async def to_code(self, w: Widget, config: dict):
        init = []
        for prop in (CONF_TEXT, CONF_PLACEHOLDER_TEXT, CONF_ACCEPTED_CHARS):
            if value := config.get(prop):
                init.extend(w.set_property(prop, await lv_text.process(value)))
        init.extend(
            w.set_property(
                CONF_MAX_LENGTH, await lv_int.process(config.get(CONF_MAX_LENGTH))
            )
        )
        init.extend(
            w.set_property(
                CONF_PASSWORD_MODE,
                await lv_bool.process(config.get(CONF_PASSWORD_MODE)),
            )
        )
        init.extend(
            w.set_property(
                CONF_ONE_LINE, await lv_bool.process(config.get(CONF_ONE_LINE))
            )
        )
        return init


textarea_spec = TextareaType()
