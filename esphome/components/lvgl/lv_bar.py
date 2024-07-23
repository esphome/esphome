import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_MODE, CONF_VALUE

from .defines import BAR_MODES, CONF_ANIMATED, CONF_BAR
from .lv_validation import animated, get_start_value, lv_float
from .types import lv_bar_t
from .widget import Widget, WidgetType

BAR_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
)

BAR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.int_,
        cv.Optional(CONF_MAX_VALUE, default=100): cv.int_,
        cv.Optional(CONF_MODE, default="NORMAL"): BAR_MODES.one_of,
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
)


class BarType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BAR, BAR_SCHEMA, BAR_MODIFY_SCHEMA)

    @property
    def w_type(self):
        return lv_bar_t

    async def to_code(self, w: Widget, config):
        var = w.obj
        init = []
        if CONF_MIN_VALUE in config:
            init.extend(
                [
                    f"lv_bar_set_range({var}, {config[CONF_MIN_VALUE]}, {config[CONF_MAX_VALUE]})",
                    f"lv_bar_set_mode({var}, {config[CONF_MODE]})",
                ]
            )
        value = await get_start_value(config)
        if value is not None:
            init.append(f"lv_bar_set_value({var}, {value}, LV_ANIM_OFF)")
        return init


bar_spec = BarType()
