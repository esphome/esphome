from esphome import automation
import esphome.codegen as cg
from esphome.components.image import Image_
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_COUNT,
    CONF_ID,
    CONF_LENGTH,
    CONF_LOCAL,
    CONF_RANGE_FROM,
    CONF_RANGE_TO,
    CONF_ROTATION,
    CONF_VALUE,
    CONF_WIDTH,
)

from . import defines as df, lv_validation as lv, types as ty
from .codegen import update_to_code
from .defines import CONF_METER
from .helpers import add_lv_use
from .lv_validation import get_end_value, get_start_value, requires_component
from .types import WIDGET_TYPES, lv_meter_t
from .widget import Widget, WidgetType, add_temp_var, get_widget

INDICATOR_LINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv.size,
        cv.Optional(CONF_COLOR, default=0): lv.lv_color,
        cv.Optional(df.CONF_R_MOD, default=0): lv.size,
        cv.Optional(CONF_VALUE): lv.lv_float,
    }
)
INDICATOR_IMG_SCHEMA = cv.Schema(
    {
        cv.Required(df.CONF_SRC): cv.All(
            cv.use_id(Image_), requires_component("image")
        ),
        cv.Required(df.CONF_PIVOT_X): lv.pixels,
        cv.Required(df.CONF_PIVOT_Y): lv.pixels,
        cv.Optional(CONF_VALUE): lv.lv_float,
    }
)
INDICATOR_ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv.size,
        cv.Optional(CONF_COLOR, default=0): lv.lv_color,
        cv.Optional(df.CONF_R_MOD, default=0): lv.size,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv.lv_float,
        cv.Exclusive(df.CONF_START_VALUE, CONF_VALUE): lv.lv_float,
        cv.Optional(df.CONF_END_VALUE): lv.lv_float,
    }
)
INDICATOR_TICKS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv.size,
        cv.Optional(df.CONF_COLOR_START, default=0): lv.lv_color,
        cv.Optional(df.CONF_COLOR_END): lv.lv_color,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv.lv_float,
        cv.Exclusive(df.CONF_START_VALUE, CONF_VALUE): lv.lv_float,
        cv.Optional(df.CONF_END_VALUE): lv.lv_float,
        cv.Optional(CONF_LOCAL, default=False): lv.lv_bool,
    }
)
INDICATOR_SCHEMA = cv.Schema(
    {
        cv.Exclusive(df.CONF_LINE, df.CONF_INDICATORS): INDICATOR_LINE_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(ty.lv_meter_indicator_t),
            }
        ),
        cv.Exclusive(df.CONF_IMAGE, df.CONF_INDICATORS): cv.All(
            INDICATOR_IMG_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(ty.lv_meter_indicator_t),
                }
            ),
            requires_component("image"),
        ),
        cv.Exclusive(df.CONF_ARC, df.CONF_INDICATORS): INDICATOR_ARC_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(ty.lv_meter_indicator_t),
            }
        ),
        cv.Exclusive(
            df.CONF_TICK_STYLE, df.CONF_INDICATORS
        ): INDICATOR_TICKS_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(ty.lv_meter_indicator_t),
            }
        ),
    }
)

SCALE_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_TICKS): cv.Schema(
            {
                cv.Optional(CONF_COUNT, default=12): cv.positive_int,
                cv.Optional(CONF_WIDTH, default=2): lv.size,
                cv.Optional(CONF_LENGTH, default=10): lv.size,
                cv.Optional(CONF_COLOR, default=0x808080): lv.lv_color,
                cv.Optional(df.CONF_MAJOR): cv.Schema(
                    {
                        cv.Optional(df.CONF_STRIDE, default=3): cv.positive_int,
                        cv.Optional(CONF_WIDTH, default=5): lv.size,
                        cv.Optional(CONF_LENGTH, default="15%"): lv.size,
                        cv.Optional(CONF_COLOR, default=0): lv.lv_color,
                        cv.Optional(df.CONF_LABEL_GAP, default=4): lv.size,
                    }
                ),
            }
        ),
        cv.Optional(CONF_RANGE_FROM, default=0.0): cv.float_,
        cv.Optional(CONF_RANGE_TO, default=100.0): cv.float_,
        cv.Optional(df.CONF_ANGLE_RANGE, default=270): cv.int_range(0, 360),
        cv.Optional(CONF_ROTATION): lv.angle,
        cv.Optional(df.CONF_INDICATORS): cv.ensure_list(INDICATOR_SCHEMA),
    }
)

METER_SCHEMA = {cv.Optional(df.CONF_SCALES): cv.ensure_list(SCALE_SCHEMA)}


class MeterType(WidgetType):
    def __init__(self):
        super().__init__(CONF_METER, METER_SCHEMA)

    @property
    def w_type(self):
        return lv_meter_t

    async def to_code(self, w: Widget, config):
        """For a meter object, create and set parameters"""

        var = w.obj
        init = []
        s = "meter_var"
        init.extend(add_temp_var("lv_meter_scale_t", s))
        for scale in config.get(df.CONF_SCALES) or ():
            rotation = 90 + (360 - scale[df.CONF_ANGLE_RANGE]) / 2
            if CONF_ROTATION in scale:
                rotation = scale[CONF_ROTATION] // 10
            init.append(f"{s} = lv_meter_add_scale({var})")
            init.append(
                f"lv_meter_set_scale_range({var}, {s}, {scale[CONF_RANGE_FROM]},"
                + f"{scale[CONF_RANGE_TO]}, {scale[df.CONF_ANGLE_RANGE]}, {rotation})",
            )
            if ticks := scale.get(df.CONF_TICKS):
                color = await lv.lv_color.process(ticks[CONF_COLOR])
                init.append(
                    f"lv_meter_set_scale_ticks({var}, {s}, {ticks[CONF_COUNT]},"
                    + f"{ticks[CONF_WIDTH]}, {ticks[CONF_LENGTH]}, {color})"
                )
                if df.CONF_MAJOR in ticks:
                    major = ticks[df.CONF_MAJOR]
                    color = await lv.lv_color.process(major[CONF_COLOR])
                    init.append(
                        f"lv_meter_set_scale_major_ticks({var}, {s}, {major[df.CONF_STRIDE]},"
                        + f"{major[CONF_WIDTH]}, {major[CONF_LENGTH]}, {color},"
                        + f"{major[df.CONF_LABEL_GAP]})"
                    )
            for indicator in scale.get(df.CONF_INDICATORS) or ():
                (t, v) = next(iter(indicator.items()))
                iid = v[CONF_ID]
                ivar = cg.new_variable(
                    iid, cg.nullptr, type_=ty.lv_meter_indicator_t_ptr
                )
                # Enable getting the meter to which this belongs.
                Widget.create(iid, var, WIDGET_TYPES["obj"], v, ivar)
                if t == df.CONF_LINE:
                    color = await lv.lv_color.process(v[CONF_COLOR])
                    init.append(
                        f"{ivar} = lv_meter_add_needle_line({var}, {s}, {v[CONF_WIDTH]},"
                        + f"{color}, {v[df.CONF_R_MOD]})"
                    )
                if t == df.CONF_ARC:
                    color = await lv.lv_color.process(v[CONF_COLOR])
                    init.append(
                        f"{ivar} = lv_meter_add_arc({var}, {s}, {v[CONF_WIDTH]},"
                        + f"{color}, {v[df.CONF_R_MOD]})"
                    )
                if t == df.CONF_TICK_STYLE:
                    color_start = await lv.lv_color.process(v[df.CONF_COLOR_START])
                    color_end = await lv.lv_color.process(
                        v.get(df.CONF_COLOR_END) or color_start
                    )
                    init.append(
                        f"{ivar} = lv_meter_add_scale_lines({var}, {s}, {color_start},"
                        + f"{color_end}, {v[CONF_LOCAL]}, {v[CONF_WIDTH]})"
                    )
                if t == df.CONF_IMAGE:
                    add_lv_use("img")
                    init.append(
                        f"{ivar} = lv_meter_add_needle_img({var}, {s}, lv_img_from({v[df.CONF_SRC]}),"
                        + f"{v[df.CONF_PIVOT_X]}, {v[df.CONF_PIVOT_Y]})"
                    )
                start_value = await get_start_value(v)
                end_value = await get_end_value(v)
                init.extend(set_indicator_values(var, ivar, start_value, end_value))

        return init


meter_spec = MeterType()


@automation.register_action(
    "lvgl.indicator.update",
    ty.ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_meter_indicator_t),
            cv.Exclusive(CONF_VALUE, CONF_VALUE): lv.lv_float,
            cv.Exclusive(df.CONF_START_VALUE, CONF_VALUE): lv.lv_float,
            cv.Optional(df.CONF_END_VALUE): lv.lv_float,
        }
    ),
)
async def indicator_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    start_value = await get_start_value(config)
    end_value = await get_end_value(config)
    init.extend(set_indicator_values(widget.var, widget.obj, start_value, end_value))
    return await update_to_code(None, action_id, widget, init, template_arg, args)


def set_indicator_values(meter, indicator, start_value, end_value):
    init = []
    if start_value is not None:
        selector = "" if end_value is None else "_start"
        init.append(
            f"lv_meter_set_indicator{selector}_value({meter}, {indicator}, {start_value})"
        )
    if end_value is not None:
        init.append(
            f"lv_meter_set_indicator_end_value({meter}, {indicator}, {end_value})"
        )
    return init
