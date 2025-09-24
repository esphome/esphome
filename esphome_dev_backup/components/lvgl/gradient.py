from esphome import config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_COLOR,
    CONF_DIRECTION,
    CONF_DITHER,
    CONF_ID,
    CONF_POSITION,
)
from esphome.cpp_generator import MockObj

from .defines import CONF_GRADIENTS, LV_DITHER, LV_GRAD_DIR, add_define
from .lv_validation import lv_color, lv_fraction
from .lvcode import lv_assign
from .types import lv_gradient_t

CONF_STOPS = "stops"


def min_stops(value):
    if len(value) < 2:
        raise cv.Invalid("Must have at least 2 stops")
    return value


GRADIENT_SCHEMA = cv.ensure_list(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(lv_gradient_t),
            cv.Optional(CONF_DIRECTION, default="NONE"): LV_GRAD_DIR.one_of,
            cv.Optional(CONF_DITHER, default="NONE"): LV_DITHER.one_of,
            cv.Required(CONF_STOPS): cv.All(
                [
                    cv.Schema(
                        {
                            cv.Required(CONF_COLOR): lv_color,
                            cv.Required(CONF_POSITION): lv_fraction,
                        }
                    )
                ],
                min_stops,
            ),
        }
    )
)


async def gradients_to_code(config):
    max_stops = 2
    for gradient in config.get(CONF_GRADIENTS, ()):
        var = MockObj(cg.new_Pvariable(gradient[CONF_ID]), "->")
        max_stops = max(max_stops, len(gradient[CONF_STOPS]))
        lv_assign(var.dir, await LV_GRAD_DIR.process(gradient[CONF_DIRECTION]))
        lv_assign(var.dither, await LV_DITHER.process(gradient[CONF_DITHER]))
        lv_assign(var.stops_count, len(gradient[CONF_STOPS]))
        for index, stop in enumerate(gradient[CONF_STOPS]):
            lv_assign(var.stops[index].color, await lv_color.process(stop[CONF_COLOR]))
            lv_assign(
                var.stops[index].frac, await lv_fraction.process(stop[CONF_POSITION])
            )
    add_define("LV_GRADIENT_MAX_STOPS", max_stops)
