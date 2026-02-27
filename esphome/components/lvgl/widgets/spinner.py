import esphome.config_validation as cv
from esphome.cpp_generator import MockObjClass

from ..defines import CONF_ARC_LENGTH, CONF_INDICATOR, CONF_MAIN, CONF_SPIN_TIME
from ..lv_validation import lv_angle_degrees, lv_milliseconds
from ..lvcode import lv_expr
from ..types import LvType
from . import Widget, WidgetType
from .arc import CONF_ARC

CONF_SPINNER = "spinner"

SPINNER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ARC_LENGTH): lv_angle_degrees,
        cv.Required(CONF_SPIN_TIME): lv_milliseconds,
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

    async def obj_creator(self, parent: MockObjClass, config: dict):
        spin_time = await lv_milliseconds.process(config[CONF_SPIN_TIME])
        arc_length = await lv_angle_degrees.process(config[CONF_ARC_LENGTH])
        return lv_expr.call("spinner_create", parent, spin_time, arc_length)


spinner_spec = SpinnerType()
