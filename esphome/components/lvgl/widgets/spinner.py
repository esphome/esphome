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
            lv_angle_degrees, cv.int_range(min=180, max=360)
        ),
        cv.Optional(CONF_SPIN_TIME, default="1s"): lv_milliseconds,
    }
)

SPINNER_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ARC_LENGTH): cv.All(
            lv_angle_degrees, cv.int_range(min=180, max=360)
        ),
        cv.Optional(CONF_SPIN_TIME): lv_milliseconds,
    }
)


class SpinnerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_SPINNER,
            LvType("lv_spinner_t"),
            (CONF_MAIN, CONF_INDICATOR),
            SPINNER_SCHEMA,
            modify_schema=SPINNER_MODIFY_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        spin_time = await lv_milliseconds.process(config.get(CONF_SPIN_TIME))
        arc_length = await lv_angle_degrees.process(config.get(CONF_ARC_LENGTH))
        if arc_length and spin_time:
            lv.spinner_set_anim_params(w.obj, spin_time, arc_length)

    def get_uses(self):
        return (CONF_ARC,)


spinner_spec = SpinnerType()
