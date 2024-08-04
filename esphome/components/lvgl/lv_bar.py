import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_MODE, CONF_VALUE

from .defines import BAR_MODES, CONF_ANIMATED, CONF_INDICATOR, CONF_MAIN, literal
from .lv_validation import animated, get_start_value, lv_float
from .lvcode import lv
from .types import LvNumber, NumberType
from .widget import Widget

CONF_BAR = "bar"
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


class BarType(NumberType):
    def __init__(self):
        super().__init__(
            CONF_BAR,
            LvNumber("lv_bar_t"),
            parts=(CONF_MAIN, CONF_INDICATOR),
            schema=BAR_SCHEMA,
            modify_schema=BAR_MODIFY_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        var = w.obj
        if CONF_MIN_VALUE in config:
            lv.bar_set_range(var, config[CONF_MIN_VALUE], config[CONF_MAX_VALUE])
            lv.bar_set_mode(var, literal(config[CONF_MODE]))
        value = await get_start_value(config)
        if value is not None:
            lv.bar_set_value(var, value, literal(config[CONF_ANIMATED]))

    @property
    def animated(self):
        return True


bar_spec = BarType()
