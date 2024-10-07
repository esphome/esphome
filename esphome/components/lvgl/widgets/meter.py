from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_COUNT,
    CONF_ID,
    CONF_ITEMS,
    CONF_LENGTH,
    CONF_LOCAL,
    CONF_RANGE_FROM,
    CONF_RANGE_TO,
    CONF_ROTATION,
    CONF_VALUE,
    CONF_WIDTH,
)

from ..automation import action_to_code
from ..defines import (
    CONF_END_VALUE,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_OPA,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_SRC,
    CONF_START_VALUE,
    CONF_TICKS,
)
from ..helpers import add_lv_use
from ..lv_validation import (
    angle,
    get_end_value,
    get_start_value,
    lv_bool,
    lv_color,
    lv_float,
    lv_image,
    opacity,
    requires_component,
    size,
)
from ..lvcode import LocalVariable, lv, lv_assign, lv_expr, lv_obj
from ..types import LvType, ObjUpdateAction
from . import Widget, WidgetType, get_widgets
from .arc import CONF_ARC
from .img import CONF_IMAGE
from .line import CONF_LINE
from .obj import obj_spec

CONF_ANGLE_RANGE = "angle_range"
CONF_COLOR_END = "color_end"
CONF_COLOR_START = "color_start"
CONF_INDICATORS = "indicators"
CONF_LABEL_GAP = "label_gap"
CONF_MAJOR = "major"
CONF_METER = "meter"
CONF_R_MOD = "r_mod"
CONF_SCALES = "scales"
CONF_STRIDE = "stride"
CONF_TICK_STYLE = "tick_style"

lv_meter_t = LvType("lv_meter_t")
lv_meter_indicator_t = cg.global_ns.struct("lv_meter_indicator_t")
lv_meter_indicator_t_ptr = lv_meter_indicator_t.operator("ptr")


def pixels(value):
    """A size in one axis in pixels"""
    if isinstance(value, str) and value.lower().endswith("px"):
        return cv.int_(value[:-2])
    return cv.int_(value)


INDICATOR_LINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): size,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_R_MOD, default=0): size,
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_OPA): opacity,
    }
)
INDICATOR_IMG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SRC): lv_image,
        cv.Required(CONF_PIVOT_X): pixels,
        cv.Required(CONF_PIVOT_Y): pixels,
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_OPA): opacity,
    }
)
INDICATOR_ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): size,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_R_MOD, default=0): size,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
        cv.Exclusive(CONF_START_VALUE, CONF_VALUE): lv_float,
        cv.Optional(CONF_END_VALUE): lv_float,
        cv.Optional(CONF_OPA): opacity,
    }
)
INDICATOR_TICKS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): size,
        cv.Optional(CONF_COLOR_START, default=0): lv_color,
        cv.Optional(CONF_COLOR_END): lv_color,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
        cv.Exclusive(CONF_START_VALUE, CONF_VALUE): lv_float,
        cv.Optional(CONF_END_VALUE): lv_float,
        cv.Optional(CONF_LOCAL, default=False): lv_bool,
    }
)
INDICATOR_SCHEMA = cv.Schema(
    {
        cv.Exclusive(CONF_LINE, CONF_INDICATORS): INDICATOR_LINE_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_t),
            }
        ),
        cv.Exclusive(CONF_IMAGE, CONF_INDICATORS): cv.All(
            INDICATOR_IMG_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(lv_meter_indicator_t),
                }
            ),
            requires_component("image"),
        ),
        cv.Exclusive(CONF_ARC, CONF_INDICATORS): INDICATOR_ARC_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_t),
            }
        ),
        cv.Exclusive(CONF_TICK_STYLE, CONF_INDICATORS): INDICATOR_TICKS_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_t),
            }
        ),
    }
)

SCALE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TICKS): cv.Schema(
            {
                cv.Optional(CONF_COUNT, default=12): cv.positive_int,
                cv.Optional(CONF_WIDTH, default=2): size,
                cv.Optional(CONF_LENGTH, default=10): size,
                cv.Optional(CONF_COLOR, default=0x808080): lv_color,
                cv.Optional(CONF_MAJOR): cv.Schema(
                    {
                        cv.Optional(CONF_STRIDE, default=3): cv.positive_int,
                        cv.Optional(CONF_WIDTH, default=5): size,
                        cv.Optional(CONF_LENGTH, default="15%"): size,
                        cv.Optional(CONF_COLOR, default=0): lv_color,
                        cv.Optional(CONF_LABEL_GAP, default=4): size,
                    }
                ),
            }
        ),
        cv.Optional(CONF_RANGE_FROM, default=0.0): cv.float_,
        cv.Optional(CONF_RANGE_TO, default=100.0): cv.float_,
        cv.Optional(CONF_ANGLE_RANGE, default=270): cv.int_range(0, 360),
        cv.Optional(CONF_ROTATION): angle,
        cv.Optional(CONF_INDICATORS): cv.ensure_list(INDICATOR_SCHEMA),
    }
)

METER_SCHEMA = {cv.Optional(CONF_SCALES): cv.ensure_list(SCALE_SCHEMA)}


class MeterType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_METER,
            lv_meter_t,
            (CONF_MAIN, CONF_INDICATOR, CONF_TICKS, CONF_ITEMS),
            METER_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        """For a meter object, create and set parameters"""

        var = w.obj
        for scale_conf in config.get(CONF_SCALES, ()):
            rotation = 90 + (360 - scale_conf[CONF_ANGLE_RANGE]) / 2
            if CONF_ROTATION in scale_conf:
                rotation = scale_conf[CONF_ROTATION] // 10
            with LocalVariable(
                "meter_var", "lv_meter_scale_t", lv_expr.meter_add_scale(var)
            ) as meter_var:
                lv.meter_set_scale_range(
                    var,
                    meter_var,
                    scale_conf[CONF_RANGE_FROM],
                    scale_conf[CONF_RANGE_TO],
                    scale_conf[CONF_ANGLE_RANGE],
                    rotation,
                )
                if ticks := scale_conf.get(CONF_TICKS):
                    color = await lv_color.process(ticks[CONF_COLOR])
                    lv.meter_set_scale_ticks(
                        var,
                        meter_var,
                        ticks[CONF_COUNT],
                        ticks[CONF_WIDTH],
                        ticks[CONF_LENGTH],
                        color,
                    )
                    if CONF_MAJOR in ticks:
                        major = ticks[CONF_MAJOR]
                        color = await lv_color.process(major[CONF_COLOR])
                        lv.meter_set_scale_major_ticks(
                            var,
                            meter_var,
                            major[CONF_STRIDE],
                            major[CONF_WIDTH],
                            major[CONF_LENGTH],
                            color,
                            major[CONF_LABEL_GAP],
                        )
                for indicator in scale_conf.get(CONF_INDICATORS, ()):
                    (t, v) = next(iter(indicator.items()))
                    iid = v[CONF_ID]
                    ivar = cg.Pvariable(iid, cg.nullptr, type_=lv_meter_indicator_t)
                    # Enable getting the meter to which this belongs.
                    wid = Widget.create(iid, var, obj_spec, v)
                    wid.obj = ivar
                    if t == CONF_LINE:
                        color = await lv_color.process(v[CONF_COLOR])
                        lv_assign(
                            ivar,
                            lv_expr.meter_add_needle_line(
                                var, meter_var, v[CONF_WIDTH], color, v[CONF_R_MOD]
                            ),
                        )
                    if t == CONF_ARC:
                        color = await lv_color.process(v[CONF_COLOR])
                        lv_assign(
                            ivar,
                            lv_expr.meter_add_arc(
                                var, meter_var, v[CONF_WIDTH], color, v[CONF_R_MOD]
                            ),
                        )
                    if t == CONF_TICK_STYLE:
                        color_start = await lv_color.process(v[CONF_COLOR_START])
                        color_end = await lv_color.process(
                            v.get(CONF_COLOR_END) or color_start
                        )
                        lv_assign(
                            ivar,
                            lv_expr.meter_add_scale_lines(
                                var,
                                meter_var,
                                color_start,
                                color_end,
                                v[CONF_LOCAL],
                                v[CONF_WIDTH],
                            ),
                        )
                    if t == CONF_IMAGE:
                        add_lv_use("img")
                        lv_assign(
                            ivar,
                            lv_expr.meter_add_needle_img(
                                var,
                                meter_var,
                                await lv_image.process(v[CONF_SRC]),
                                v[CONF_PIVOT_X],
                                v[CONF_PIVOT_Y],
                            ),
                        )
                    await set_indicator_values(var, ivar, v)


meter_spec = MeterType()


@automation.register_action(
    "lvgl.indicator.update",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_meter_indicator_t),
            cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
            cv.Exclusive(CONF_START_VALUE, CONF_VALUE): lv_float,
            cv.Optional(CONF_END_VALUE): lv_float,
            cv.Optional(CONF_OPA): opacity,
        }
    ),
)
async def indicator_update_to_code(config, action_id, template_arg, args):
    widget = await get_widgets(config)

    async def set_value(w: Widget):
        await set_indicator_values(w.var, w.obj, config)

    return await action_to_code(widget, set_value, action_id, template_arg, args)


async def set_indicator_values(meter, indicator, config):
    start_value = await get_start_value(config)
    end_value = await get_end_value(config)
    if start_value is not None:
        if end_value is None:
            lv.meter_set_indicator_value(meter, indicator, start_value)
        else:
            lv.meter_set_indicator_start_value(meter, indicator, start_value)
    if end_value is not None:
        lv.meter_set_indicator_end_value(meter, indicator, end_value)
    if opa := config.get(CONF_OPA):
        lv_assign(indicator.opa, await opacity.process(opa))
        lv_obj.invalidate(meter)
