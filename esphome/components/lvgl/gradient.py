from operator import itemgetter

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

from .defines import (
    CONF_END_ANGLE,
    CONF_GRADIENTS,
    CONF_OPA,
    CONF_START_ANGLE,
    LV_DITHER,
    LV_GRAD_EXTEND,
    add_define,
    add_lv_use,
    add_warning,
)
from .lv_validation import (
    lv_angle_degrees,
    lv_color,
    lv_percentage,
    opacity,
    pixels_or_percent,
)
from .lvcode import lv
from .types import lv_color_t, lv_gradient_t, lv_opa_t

CONF_STOPS = "stops"
CONF_LINEAR = "linear"
CONF_RADIAL = "radial"
CONF_CONICAL = "conical"
CONF_EXTEND = "extend"
CONF_FROM_X = "from_x"
CONF_FROM_Y = "from_y"
CONF_TO_X = "to_x"
CONF_TO_Y = "to_y"
CONF_CENTER_X = "center_x"
CONF_CENTER_Y = "center_y"
CONF_FOCAL_X = "focal_x"
CONF_FOCAL_Y = "focal_y"
CONF_FOCAL_RADIUS = "focal_radius"


def min_stops(value):
    if len(value) < 2:
        raise cv.Invalid("Must have at least 2 stops")
    return value


STOPS_SCHEMA = cv.All(
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
)

LINEAR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FROM_X): pixels_or_percent,
        cv.Required(CONF_FROM_Y): pixels_or_percent,
        cv.Required(CONF_TO_X): pixels_or_percent,
        cv.Required(CONF_TO_Y): pixels_or_percent,
        cv.Optional(CONF_EXTEND, default="PAD"): LV_GRAD_EXTEND.one_of,
    }
)

RADIAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CENTER_X): pixels_or_percent,
        cv.Required(CONF_CENTER_Y): pixels_or_percent,
        cv.Required(CONF_TO_X): pixels_or_percent,
        cv.Required(CONF_TO_Y): pixels_or_percent,
        cv.Optional(CONF_FOCAL_X): pixels_or_percent,
        cv.Optional(CONF_FOCAL_Y): pixels_or_percent,
        # No default: gradient_validator() must be able to tell whether this was actually
        # given, to require it alongside focal_x/focal_y rather than silently drop it.
        # LVGL's lv_grad_radial_set_focal() takes this as a scalar, not lv_pct() -
        # unlike every other coordinate here, a percentage is not accepted.
        cv.Optional(CONF_FOCAL_RADIUS): cv.positive_int,
        cv.Optional(CONF_EXTEND, default="PAD"): LV_GRAD_EXTEND.one_of,
    }
)

CONICAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CENTER_X): pixels_or_percent,
        cv.Required(CONF_CENTER_Y): pixels_or_percent,
        cv.Optional(CONF_START_ANGLE, default=0): lv_angle_degrees,
        cv.Optional(CONF_END_ANGLE, default=360): lv_angle_degrees,
        cv.Optional(CONF_EXTEND, default="PAD"): LV_GRAD_EXTEND.one_of,
    }
)


def gradient_validator(config):
    direction = config[CONF_DIRECTION]
    for gradient_direction, key in (
        ("LINEAR", CONF_LINEAR),
        ("RADIAL", CONF_RADIAL),
        ("CONICAL", CONF_CONICAL),
    ):
        if direction == gradient_direction:
            if key not in config:
                raise cv.Invalid(
                    f"'{key}' is required for {gradient_direction} gradient direction"
                )
        elif key in config:
            raise cv.Invalid(
                f"'{key}' is only valid with 'direction: {gradient_direction}'"
            )
    if CONF_RADIAL in config:
        radial = config[CONF_RADIAL]
        has_focal_x = CONF_FOCAL_X in radial
        has_focal_y = CONF_FOCAL_Y in radial
        has_focal_radius = CONF_FOCAL_RADIUS in radial
        if has_focal_x != has_focal_y or (has_focal_radius and not has_focal_x):
            raise cv.Invalid(
                "'focal_x', 'focal_y' and 'focal_radius' must be specified together "
                "in 'radial'"
            )
    return config


GRADIENT_SCHEMA = cv.ensure_list(
    cv.All(
        cv.Schema(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(lv_gradient_t),
                cv.Required(CONF_DIRECTION): cv.one_of(
                    "HOR",
                    "HORIZONTAL",
                    "VER",
                    "VERTICAL",
                    "LINEAR",
                    "RADIAL",
                    "CONICAL",
                    upper=True,
                ),
                cv.Optional(CONF_DITHER): LV_DITHER.one_of,
                cv.Optional(CONF_LINEAR): LINEAR_SCHEMA,
                cv.Optional(CONF_RADIAL): RADIAL_SCHEMA,
                cv.Optional(CONF_CONICAL): CONICAL_SCHEMA,
                cv.Required(CONF_STOPS): STOPS_SCHEMA,
            }
        ),
        gradient_validator,
    )
)


async def gradients_to_code(config):
    add_lv_use("gradient")
    max_stops = 2
    if any(CONF_DITHER in x for x in config.get(CONF_GRADIENTS, ())):
        add_warning(
            "The 'dither' option for gradients is not supported by LVGL 9.x and will be ignored"
        )
    if any(
        x[CONF_DIRECTION] in ("LINEAR", "RADIAL", "CONICAL")
        for x in config.get(CONF_GRADIENTS, ())
    ):
        # LVGL's software renderer only draws these gradient types when this is enabled; without
        # it they silently fall back to a plain horizontal gradient.
        add_define("LV_USE_DRAW_SW_COMPLEX_GRADIENTS")
    for gradient in config.get(CONF_GRADIENTS, ()):
        var = MockObj(cg.new_Pvariable(gradient[CONF_ID]), "->")
        idbase = gradient[CONF_ID].id
        stops = sorted(gradient[CONF_STOPS], key=itemgetter(CONF_POSITION))
        max_stops = max(max_stops, len(stops))
        direction = gradient[CONF_DIRECTION]
        if direction.startswith("VER"):
            lv.grad_vertical_init(var)
        elif direction.startswith("HOR"):
            lv.grad_horizontal_init(var)
        elif direction == "LINEAR":
            linear = gradient[CONF_LINEAR]
            lv.grad_linear_init(
                var,
                await pixels_or_percent.process(linear[CONF_FROM_X]),
                await pixels_or_percent.process(linear[CONF_FROM_Y]),
                await pixels_or_percent.process(linear[CONF_TO_X]),
                await pixels_or_percent.process(linear[CONF_TO_Y]),
                await LV_GRAD_EXTEND.process(linear[CONF_EXTEND]),
            )
        elif direction == "RADIAL":
            radial = gradient[CONF_RADIAL]
            lv.grad_radial_init(
                var,
                await pixels_or_percent.process(radial[CONF_CENTER_X]),
                await pixels_or_percent.process(radial[CONF_CENTER_Y]),
                await pixels_or_percent.process(radial[CONF_TO_X]),
                await pixels_or_percent.process(radial[CONF_TO_Y]),
                await LV_GRAD_EXTEND.process(radial[CONF_EXTEND]),
            )
            if CONF_FOCAL_X in radial:
                lv.grad_radial_set_focal(
                    var,
                    await pixels_or_percent.process(radial[CONF_FOCAL_X]),
                    await pixels_or_percent.process(radial[CONF_FOCAL_Y]),
                    radial.get(CONF_FOCAL_RADIUS, 0),
                )
        elif direction == "CONICAL":
            conical = gradient[CONF_CONICAL]
            lv.grad_conical_init(
                var,
                await pixels_or_percent.process(conical[CONF_CENTER_X]),
                await pixels_or_percent.process(conical[CONF_CENTER_Y]),
                await lv_angle_degrees.process(conical[CONF_START_ANGLE]),
                await lv_angle_degrees.process(conical[CONF_END_ANGLE]),
                await LV_GRAD_EXTEND.process(conical[CONF_EXTEND]),
            )
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
