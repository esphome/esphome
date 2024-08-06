import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_COLOR, CONF_LED

from .defines import CONF_MAIN
from .lv_validation import lv_brightness, lv_color
from .lvcode import lv
from .types import LvType
from .widget import Widget, WidgetType

LED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_BRIGHTNESS): lv_brightness,
    }
)


class LedType(WidgetType):
    def __init__(self):
        super().__init__(CONF_LED, LvType("lv_led_t"), (CONF_MAIN,), LED_SCHEMA)

    async def to_code(self, w: Widget, config):
        if (color := config.get(CONF_COLOR)) is not None:
            lv.led_set_color(w.obj, await lv_color.process(color))
        if (brightness := config.get(CONF_BRIGHTNESS)) is not None:
            lv.led_set_brightness(w.obj, await lv_brightness.process(brightness))


led_spec = LedType()
