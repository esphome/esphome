import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import Lambda

from ..defines import CONF_MAIN, CONF_X, CONF_Y, call_lambda
from ..lvcode import lv_add
from ..schemas import point_schema
from ..types import LvCompound, LvType
from . import Widget, WidgetType

CONF_LINE = "line"
CONF_POINTS = "points"
CONF_POINT_LIST_ID = "point_list_id"

lv_point_t = cg.global_ns.struct("lv_point_t")


LINE_SCHEMA = {
    cv.Required(CONF_POINTS): cv.ensure_list(point_schema),
}


async def process_coord(coord):
    if isinstance(coord, Lambda):
        coord = call_lambda(
            await cg.process_lambda(coord, [], return_type="lv_coord_t")
        )
        if not coord.endswith("()"):
            coord = f"static_cast<lv_coord_t>({coord})"
        return cg.RawExpression(coord)
    return cg.safe_exp(coord)


class LineType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_LINE,
            LvType("LvLineType", parents=(LvCompound,)),
            (CONF_MAIN,),
            LINE_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        points = [
            [await process_coord(p[CONF_X]), await process_coord(p[CONF_Y])]
            for p in config[CONF_POINTS]
        ]
        lv_add(w.var.set_points(points))


line_spec = LineType()
