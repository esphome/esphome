import esphome.config_validation as cv
from esphome.cpp_generator import MockObjClass

from .arc import CONF_ARC
from .defines import CONF_ARC_LENGTH, CONF_INDICATOR, CONF_MAIN, CONF_SPIN_TIME
from .lv_validation import angle
from .lvcode import lv_expr
from .types import LvType
from .widget import Widget, WidgetType

CONF_SPINNER = "spinner"

SPINNER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ARC_LENGTH): angle,
        cv.Required(CONF_SPIN_TIME): cv.positive_time_period_milliseconds,
    }
)


class SpinnerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_SPINNER,
            LvType("lv_spinner_t"),
            (CONF_MAIN, CONF_INDICATOR),
            SPINNER_SCHEMA,
            {},
        )

    async def to_code(self, w: Widget, config):
        return []

    def get_uses(self):
        return (CONF_ARC,)

    def obj_creator(self, parent: MockObjClass, config: dict):
        spin_time = config[CONF_SPIN_TIME].total_milliseconds
        arc_length = config[CONF_ARC_LENGTH] // 10
        return lv_expr.call("spinner_create", parent, spin_time, arc_length)


spinner_spec = SpinnerType()
