import esphome.config_validation as cv

from ..defines import CONF_ARC_LENGTH, CONF_INDICATOR, CONF_MAIN, CONF_SPIN_TIME
from ..lv_validation import lv_angle_degrees, lv_milliseconds
from ..lvcode import lv
from ..types import LvType
from . import Widget, WidgetType
from .arc import CONF_ARC

CONF_SPINNER = "spinner"

SPINNER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ARC_LENGTH, default=200): cv.All(
            lv_angle_degrees, cv.int_range(min=0, max=360)
        ),
        cv.Optional(CONF_SPIN_TIME, default="2s"): lv_milliseconds,
    }
)


class SpinnerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_SPINNER,
            LvType("lv_spinner_t"),
            (CONF_MAIN, CONF_INDICATOR),
            SPINNER_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        spin_time = await lv_milliseconds.process(config.get(CONF_SPIN_TIME))
        arc_length = int(config[CONF_ARC_LENGTH])
        if arc_length < 180:
            arc_length += 180
        lv.spinner_set_anim_params(w.obj, spin_time, arc_length)

    def get_uses(self):
        return (CONF_ARC,)


spinner_spec = SpinnerType()
