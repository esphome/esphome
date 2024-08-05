import functools

import esphome.codegen as cg
import esphome.config_validation as cv

from . import defines as df
from .defines import CONF_MAIN, literal
from .lvcode import lv
from .types import LvType
from .widget import Widget, WidgetType

CONF_LINE = "line"
CONF_POINTS = "points"
CONF_POINT_LIST_ID = "point_list_id"

lv_point_t = cg.global_ns.struct("lv_point_t")


def point_list(il):
    il = cv.string(il)
    nl = il.replace(" ", "").split(",")
    return [int(n) for n in nl]


def cv_point_list(value):
    if not isinstance(value, list):
        raise cv.Invalid("List of points required")
    values = [point_list(v) for v in value]
    if not functools.reduce(lambda f, v: f and len(v) == 2, values, True):
        raise cv.Invalid("Points must be a list of x,y integer pairs")
    return values


LINE_SCHEMA = {
    cv.Required(df.CONF_POINTS): cv_point_list,
    cv.GenerateID(CONF_POINT_LIST_ID): cv.declare_id(lv_point_t),
}


class LineType(WidgetType):
    def __init__(self):
        super().__init__(CONF_LINE, LvType("lv_line_t"), (CONF_MAIN,), LINE_SCHEMA)

    async def to_code(self, w: Widget, config):
        """For a line object, create and add the points"""
        data = literal(config[CONF_POINTS])
        points = cg.static_const_array(config[CONF_POINT_LIST_ID], data)
        lv.line_set_points(w.obj, points, len(data))


line_spec = LineType()
