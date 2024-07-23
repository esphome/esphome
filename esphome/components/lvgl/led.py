import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_COLOR, CONF_LED

from .lv_validation import lv_brightness, lv_color
from .types import lv_led_t
from .widget import Widget, WidgetType

LED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_BRIGHTNESS): lv_brightness,
    }
)


class LedType(WidgetType):
    def __init__(self):
        super().__init__(CONF_LED, LED_SCHEMA)

    @property
    def w_type(self):
        return lv_led_t

    async def to_code(self, w: Widget, config):
        init = []
        init.extend(
            w.set_property(CONF_COLOR, await lv_color.process(config[CONF_COLOR]))
        )
        init.extend(
            w.set_property(
                CONF_BRIGHTNESS,
                await lv_brightness.process(config.get(CONF_BRIGHTNESS)),
            )
        )
        return init


led_spec = LedType()
