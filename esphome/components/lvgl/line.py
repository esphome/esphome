import functools

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import defines as df, lv_validation as lv
from .defines import CONF_LINE
from .helpers import add_lv_use
from .types import generate_id, lv_line_t, lv_point_t
from .widget import Widget, WidgetType


def cv_point_list(value):
    if not isinstance(value, list):
        raise cv.Invalid("List of points required")
    values = list(map(lv.point_list, value))
    if not functools.reduce(lambda f, v: f and len(v) == 2, values, True):
        raise cv.Invalid("Points must be a list of x,y integer pairs")
    add_lv_use("POINT")
    return {
        CONF_ID: cv.declare_id(lv_point_t)(generate_id(df.CONF_POINTS)),
        df.CONF_POINTS: values,
    }


LINE_SCHEMA = {cv.Optional(df.CONF_POINTS): cv_point_list}


class LineType(WidgetType):
    def __init__(self):
        super().__init__(CONF_LINE, LINE_SCHEMA)

    @property
    def w_type(self):
        return lv_line_t

    async def to_code(self, w: Widget, config):
        """For a line object, create and add the points"""
        data = config[df.CONF_POINTS]
        point_list = data[df.CONF_POINTS]
        points = cg.static_const_array(data[CONF_ID], point_list)
        return [f"lv_line_set_points({w.obj}, {points}, {len(point_list)})"]


line_spec = LineType()
