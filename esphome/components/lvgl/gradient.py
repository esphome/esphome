from esphome import config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_COLOR,
    CONF_DIRECTION,
    CONF_DITHER,
    CONF_ID,
    CONF_POSITION,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj

from .defines import CONF_GRADIENTS, CONF_OPA, LV_DITHER, add_define, add_warning
from .lv_validation import lv_color, lv_percentage, opacity
from .lvcode import lv
from .types import lv_color_t, lv_gradient_t, lv_opa_t

CONF_STOPS = "stops"


def min_stops(value):
    if len(value) < 2:
        raise cv.Invalid("Must have at least 2 stops")
    return value


GRADIENT_SCHEMA = cv.ensure_list(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(lv_gradient_t),
            cv.Required(CONF_DIRECTION): cv.one_of(
                "HOR", "HORIZONTAL", "VER", "VERTICAL", upper=True
            ),
            cv.Optional(CONF_DITHER, default="NONE"): LV_DITHER.one_of,
            cv.Required(CONF_STOPS): cv.All(
                [
                    cv.Schema(
                        {
                            cv.Required(CONF_COLOR): lv_color,
                            cv.Optional(CONF_OPA, default=1.0): opacity,
                            cv.Required(CONF_POSITION): lv_percentage,
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
    if any(CONF_DITHER in x for x in config.get(CONF_GRADIENTS, ())):
        add_warning(
            "The 'dither' option for gradients is not supported by LVGL 9.x and will be ignored"
        )
    for gradient in config.get(CONF_GRADIENTS, ()):
        var = MockObj(cg.new_Pvariable(gradient[CONF_ID]), "->")
        idbase = gradient[CONF_ID].id
        stops = gradient[CONF_STOPS]
        max_stops = max(max_stops, len(stops))
        if gradient[CONF_DIRECTION].startswith("VER"):
            lv.grad_vertical_init(var)
        else:
            lv.grad_horizontal_init(var)
        stop_colors = cg.static_const_array(
            ID(idbase + "_colors_", type=lv_color_t),
            [await lv_color.process(x[CONF_COLOR]) for x in stops],
        )
        stop_opacities = cg.static_const_array(
            ID(idbase + "_opacities_", type=lv_opa_t),
            [await opacity.process(x[CONF_OPA]) for x in stops],
        )
        stop_positions = cg.static_const_array(
            ID(idbase + "_positions_", type=cg.uint8),
            [await lv_percentage.process(x[CONF_POSITION]) for x in stops],
        )
        lv.grad_init_stops(var, stop_colors, stop_opacities, stop_positions, len(stops))

    add_define("LV_GRADIENT_MAX_STOPS", max_stops)
