import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_X, CONF_Y

from ..defines import CONF_MAIN
from ..lv_validation import pixels_or_percent
from ..lvcode import lv_add
from ..schemas import point_schema
from ..types import LvCompound, LvType
from . import Widget, WidgetType

CONF_LINE = "line"
CONF_POINTS = "points"
CONF_POINT_LIST_ID = "point_list_id"

lv_point_t = cg.global_ns.struct("lv_point_t")
lv_point_precise_t = cg.global_ns.struct("lv_point_precise_t")


class LineType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_LINE,
            LvType("LvLineType", parents=(LvCompound,)),
            (CONF_MAIN,),
            schema={cv.Required(CONF_POINTS): cv.ensure_list(point_schema)},
            modify_schema={cv.Optional(CONF_POINTS): cv.ensure_list(point_schema)},
        )

    async def to_code(self, w: Widget, config):
        if CONF_POINTS in config:
            points = [
                [
                    await pixels_or_percent.process(p[CONF_X]),
                    await pixels_or_percent.process(p[CONF_Y]),
                ]
                for p in config[CONF_POINTS]
            ]
            lv_add(w.var.set_points(points))


line_spec = LineType()
