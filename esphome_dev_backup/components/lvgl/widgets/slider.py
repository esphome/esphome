import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_MODE, CONF_VALUE

from ..defines import (
    BAR_MODES,
    CONF_ANIMATED,
    CONF_INDICATOR,
    CONF_KNOB,
    CONF_MAIN,
    literal,
)
from ..helpers import add_lv_use
from ..lv_validation import animated, get_start_value, lv_float
from ..lvcode import lv
from ..types import LvNumber, NumberType
from . import Widget
from .lv_bar import CONF_BAR

CONF_SLIDER = "slider"
SLIDER_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
)

SLIDER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.int_,
        cv.Optional(CONF_MAX_VALUE, default=100): cv.int_,
        cv.Optional(CONF_MODE, default="NORMAL"): BAR_MODES.one_of,
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
)


class SliderType(NumberType):
    def __init__(self):
        super().__init__(
            CONF_SLIDER,
            LvNumber("lv_slider_t"),
            parts=(CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
            schema=SLIDER_SCHEMA,
            modify_schema=SLIDER_MODIFY_SCHEMA,
        )

    @property
    def animated(self):
        return True

    async def to_code(self, w: Widget, config):
        add_lv_use(CONF_BAR)
        if CONF_MIN_VALUE in config:
            # not modify case
            lv.slider_set_range(w.obj, config[CONF_MIN_VALUE], config[CONF_MAX_VALUE])
            lv.slider_set_mode(w.obj, literal(config[CONF_MODE]))
        value = await get_start_value(config)
        if value is not None:
            lv.slider_set_value(w.obj, value, literal(config[CONF_ANIMATED]))


slider_spec = SliderType()
