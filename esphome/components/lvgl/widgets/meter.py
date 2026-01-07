from esphome import automation
import esphome.codegen as cg
from esphome.components.image import DOMAIN as IMAGE_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_COUNT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_ITEMS,
    CONF_LENGTH,
    CONF_LOCAL,
    CONF_RANGE_FROM,
    CONF_RANGE_TO,
    CONF_ROTATION,
    CONF_VALUE,
    CONF_WIDTH,
    CONF_X,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.cpp_types import nullptr

from .. import obj_spec, set_obj_properties
from ..automation import action_to_code
from ..defines import (
    CHILD_ALIGNMENTS,
    CONF_ALIGN,
    CONF_CONTAINER,
    CONF_END_VALUE,
    CONF_INDICATOR,
    CONF_LINE_WIDTH,
    CONF_MAIN,
    CONF_OPA,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_RADIUS,
    CONF_SCALE,
    CONF_SRC,
    CONF_START_VALUE,
    CONF_TICKS,
    LV_OBJ_FLAG,
    LV_PART,
    LV_SCALE_MODE,
    get_remapped_uses,
)
from ..helpers import add_lv_use, lvgl_components_required
from ..lv_validation import (
    LV_OPA,
    LV_RADIUS,
    get_end_value,
    get_start_value,
    lv_angle_degrees,
    lv_bool,
    lv_color,
    lv_float,
    lv_image,
    lv_int,
    opacity,
    padding,
    pixels,
    pixels_or_percent,
    pixels_or_percent_validator,
    requires_component,
    size,
)
from ..lvcode import LambdaContext, LocalVariable, lv, lv_add, lv_expr, lv_obj
from ..schemas import STATE_SCHEMA
from ..styles import LVStyle
from ..types import (
    LV_EVENT,
    LvCompound,
    LvType,
    ObjUpdateAction,
    lv_event_t,
    lv_img_t,
    lv_obj_t,
)
from . import Widget, WidgetType, get_widgets, widget_to_code
from .arc import CONF_ARC
from .img import CONF_IMAGE
from .line import CONF_LINE

CONF_ANGLE_RANGE = "angle_range"
CONF_COLOR_END = "color_end"
CONF_COLOR_START = "color_start"
CONF_IMAGE_ID = "image_id"
CONF_INDICATORS = "indicators"
CONF_LINE_ID = "line_id"
CONF_LABEL_GAP = "label_gap"
CONF_MAJOR = "major"
CONF_METER = "meter"
CONF_PADDING = "padding"
CONF_PIVOT = "pivot"
CONF_R_MOD = "r_mod"
CONF_RADIAL_OFFSET = "radial_offset"
CONF_SCALES = "scales"
CONF_STRIDE = "stride"
CONF_TICK_STYLE = "tick_style"

# LVGL 9.4 Migration: Use scale widget instead of removed meter widget
#
# The lv_meter widget was removed in LVGL 9.4 and replaced with the more
# flexible lv_scale widget. This implementation emulates meter functionality
# using the scale widget with the following mappings:
#
# - lv_meter -> lv_scale (set to LV_SCALE_MODE_ROUND_OUTER for circular meters)
# - lv_meter_scale -> scale configuration (range, ticks, etc.)
# - lv_meter_indicator -> lv_scale_section (colored ranges on the scale)
#


# For compatibility, keep meter types but map to scale
lv_scale_t = LvType("lv_scale_t")
lv_meter_t = lv_obj_t
lv_meter_indicator_t = lv_obj_t
lv_meter_indicator_ticks_t = LvType(
    "lv_scale_section_t", parents=(lv_meter_indicator_t,)
)
lv_meter_indicator_arc_t = LvType("lv_scale_section_t", parents=(lv_meter_indicator_t,))
lv_meter_indicator_line_t = LvType(
    "IndicatorLine",
    parents=(
        LvCompound,
        lv_meter_indicator_t,
    ),
)
lv_meter_indicator_image_t = LvType("lv_image_t", parents=(lv_meter_indicator_t,))

DEFAULT_LABEL_GAP = 10  # Default label gap for major ticks added by LVGL

INDICATOR_LINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): cv.int_,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_R_MOD): padding,
        cv.Optional(CONF_LENGTH): pixels_or_percent_validator,
        cv.Optional(CONF_RADIAL_OFFSET, 0): pixels_or_percent_validator,
        cv.Optional(CONF_VALUE, default=0.0): lv_float,
        cv.Optional(CONF_OPA, default=1.0): opacity,
    }
).add_extra(cv.has_at_most_one_key(CONF_R_MOD, CONF_LENGTH))


class ScaleType(WidgetType):
    """
    Will migrate to scale.py in due course
    """

    def __init__(self):
        super().__init__(
            CONF_SCALE,
            lv_scale_t,
            (CONF_MAIN, CONF_ITEMS, CONF_INDICATOR),
            {},
            is_mock=True,
        )


scale_spec = ScaleType()

INDICATOR_IMG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SRC): lv_image,
        cv.Optional(CONF_PIVOT_X, default=0): pixels,
        cv.Optional(CONF_PIVOT_Y): pixels,
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_OPA, default=1.0): opacity,
    }
)
INDICATOR_ARC_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_WIDTH, default=4): cv.int_,
            cv.Optional(CONF_COLOR, default=0): lv_color,
            cv.Optional(CONF_R_MOD): padding,
            cv.Optional(CONF_PADDING): padding,
            cv.Optional(CONF_VALUE): lv_float,
            cv.Optional(CONF_START_VALUE): lv_float,
            cv.Optional(CONF_END_VALUE): lv_float,
            cv.Optional(CONF_OPA): opacity,
        }
    )
    .add_extra(cv.has_at_most_one_key(CONF_R_MOD, CONF_PADDING))
    .add_extra(cv.has_at_most_one_key(CONF_VALUE, CONF_START_VALUE))
)

INDICATOR_TICKS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): cv.int_,
        cv.Optional(CONF_COLOR_START, default=0): lv_color,
        cv.Optional(CONF_COLOR_END): lv_color,
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_START_VALUE): lv_float,
        cv.Optional(CONF_END_VALUE): lv_float,
        cv.Optional(CONF_LOCAL, default=False): lv_bool,
    }
).add_extra(cv.has_at_most_one_key(CONF_VALUE, CONF_START_VALUE))

INDICATOR_SCHEMA = cv.Schema(
    {
        cv.Exclusive(CONF_LINE, CONF_INDICATORS): INDICATOR_LINE_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_line_t),
            }
        ),
        cv.Exclusive(CONF_IMAGE, CONF_INDICATORS): cv.All(
            INDICATOR_IMG_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(lv_meter_indicator_image_t),
                    cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(lv_img_t),
                }
            ),
            requires_component("image"),
        ),
        cv.Exclusive(CONF_ARC, CONF_INDICATORS): INDICATOR_ARC_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_arc_t),
            }
        ),
        cv.Exclusive(CONF_TICK_STYLE, CONF_INDICATORS): INDICATOR_TICKS_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_ticks_t),
            }
        ),
    }
)

SCALE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(lv_obj_t),
        cv.Optional(CONF_TICKS): cv.Schema(
            {
                cv.Optional(CONF_COUNT, default=12): cv.positive_int,
                cv.Optional(CONF_WIDTH, default=2): cv.positive_int,
                cv.Optional(CONF_LENGTH, default=10): size,
                cv.Optional(CONF_RADIAL_OFFSET, default=0): size,
                cv.Optional(CONF_COLOR, default=0x808080): lv_color,
                cv.Optional(CONF_MAJOR): cv.Schema(
                    {
                        cv.Optional(CONF_STRIDE, default=3): cv.positive_int,
                        cv.Optional(CONF_WIDTH, default=5): size,
                        cv.Optional(CONF_LENGTH, default="15%"): size,
                        cv.Optional(CONF_RADIAL_OFFSET, default=0): size,
                        cv.Optional(CONF_COLOR, default=0): lv_color,
                        cv.Optional(CONF_LABEL_GAP, default=4): size,
                    }
                ),
            }
        ),
        cv.Optional(CONF_RANGE_FROM, default=0.0): lv_int,
        cv.Optional(CONF_RANGE_TO, default=100.0): lv_int,
        cv.Optional(CONF_ANGLE_RANGE, default=270): lv_angle_degrees,
        cv.Optional(CONF_ROTATION, default=0): lv_angle_degrees,
        cv.Optional(CONF_INDICATORS): cv.ensure_list(INDICATOR_SCHEMA),
    }
)

METER_SCHEMA = {
    cv.Optional(CONF_PIVOT): STATE_SCHEMA,
    cv.Optional(CONF_INDICATOR): STATE_SCHEMA,
    cv.Optional(CONF_SCALES): cv.ensure_list(SCALE_SCHEMA),
}

LIGHT_STYLE = LVStyle(
    "lv_meter_light",
    {
        "bg_opa": 1.0,
        "bg_color": 0xEEEEEE,
        "line_width": 1,
        "line_color": 0xEEEEEE,
        "arc_width": 2,
        "arc_color": 0xEEEEEE,
        "pad_all": 10,
        "border_width": 2,
        "border_color": 0xEEEEEE,
        "radius": "LV_RADIUS_CIRCLE",
    },
)

PIVOT_STYLE = {
    CONF_RADIUS: LV_RADIUS.CIRCLE,
    CONF_ALIGN: CHILD_ALIGNMENTS.CENTER,
    "bg_color": 0x000000,
    "bg_opa": 1.0,
    CONF_WIDTH: 15,
    CONF_HEIGHT: 15,
}


line_indicator_type = WidgetType(
    CONF_INDICATOR,
    lv_meter_indicator_line_t,
    (CONF_MAIN,),
    lv_name=CONF_LINE,
    is_mock=True,
)

arc_indicator_type = WidgetType(
    CONF_INDICATOR,
    lv_meter_indicator_arc_t,
    (CONF_MAIN,),
    lv_name=CONF_ARC,
    is_mock=True,
)

image_indicator_type = WidgetType(
    CONF_INDICATOR,
    lv_meter_indicator_image_t,
    (CONF_MAIN,),
    lv_name=CONF_IMAGE,
    is_mock=True,
)


class MeterType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_METER,
            lv_obj_t,  # Use scale widget instead of meter
            # Note that mapping from 8.x to 9.x, indicator styling is applied to needles, and tick styling
            # is migrated to indicator
            (CONF_MAIN, CONF_INDICATOR, CONF_TICKS, CONF_ITEMS),
            METER_SCHEMA,
            lv_name=CONF_CONTAINER,
        )

    def validate(self, value):
        return cv.has_at_most_one_key(CONF_INDICATOR, CONF_PIVOT)(value)

    async def on_create(self, var: MockObj, config: dict):
        # Remove theme styling from outer container
        lv.obj_add_style(var, await LIGHT_STYLE.get_var(), LV_PART.MAIN)

    async def create_to_code(self, config: dict, parent: MockObj):
        """For a meter object using scale widget, create and set parameters"""

        lvgl_components_required.add("scale")  # Use scale component
        outer_config = config.copy()
        indicator_config = {CONF_INDICATOR: outer_config.pop(CONF_TICKS, {})}
        w = await super().create_to_code(outer_config, parent)
        var = w.obj

        # LVGL 9.4 scale widget setup
        # Background style will be applied.
        for scale_conf in config.get(CONF_SCALES, ()):
            scale_var = cg.Pvariable(scale_conf[CONF_ID], lv_expr.scale_create(var))
            percent100 = await pixels_or_percent.process(1.0)
            lv_obj.set_style_height(scale_var, percent100, LV_PART.MAIN)
            lv_obj.set_style_width(scale_var, percent100, LV_PART.MAIN)
            lv_obj.set_style_align(scale_var, CHILD_ALIGNMENTS.CENTER, LV_PART.MAIN)
            lv_obj.set_style_bg_opa(scale_var, LV_OPA.TRANSP, LV_PART.MAIN)
            lv_obj.set_style_radius(scale_var, LV_RADIUS.CIRCLE, 0)
            await set_obj_properties(Widget(scale_var, scale_spec), indicator_config)

            lv.scale_set_mode(scale_var, LV_SCALE_MODE.ROUND_INNER)
            # Set the scale range
            range_from = await lv_int.process(scale_conf[CONF_RANGE_FROM])
            range_to = await lv_int.process(scale_conf[CONF_RANGE_TO])
            lv.scale_set_range(scale_var, range_from, range_to)

            angle_range = await lv_angle_degrees.process(scale_conf[CONF_ANGLE_RANGE])
            rotation = await lv_angle_degrees.process(scale_conf[CONF_ROTATION])
            # Set angle range
            lv.scale_set_angle_range(
                scale_var,
                angle_range,
            )

            # Set rotation if specified
            if rotation:
                lv.scale_set_rotation(scale_var, rotation)

            if ticks := scale_conf.get(CONF_TICKS):
                # Set total tick count
                lv.scale_set_total_tick_count(scale_var, ticks[CONF_COUNT])

                # Set tick styling
                lv_obj.set_style_length(
                    scale_var, await size.process(ticks[CONF_LENGTH]), LV_PART.ITEMS
                )
                lv_obj.set_style_line_width(
                    scale_var, await size.process(ticks[CONF_WIDTH]), LV_PART.ITEMS
                )
                lv_obj.set_style_radial_offset(
                    scale_var,
                    await size.process(ticks[CONF_RADIAL_OFFSET]),
                    LV_PART.ITEMS,
                )
                lv_obj.set_style_line_color(
                    scale_var,
                    await lv_color.process(ticks[CONF_COLOR]),
                    LV_PART.ITEMS,
                )

                # Hide the scale line
                lv.obj_set_style_arc_opa(scale_var, LV_OPA.TRANSP, LV_PART.MAIN)
                if CONF_MAJOR in ticks:
                    major = ticks[CONF_MAJOR]
                    # Set major tick frequency
                    lv.scale_set_major_tick_every(scale_var, major[CONF_STRIDE])

                    # Enable labels for major ticks
                    lv.scale_set_label_show(scale_var, True)

                    # Set major tick styling
                    lv_obj.set_style_length(
                        scale_var,
                        await size.process(major[CONF_LENGTH]),
                        LV_PART.INDICATOR,
                    )
                    lv_obj.set_style_radial_offset(
                        scale_var,
                        await size.process(ticks[CONF_RADIAL_OFFSET]),
                        LV_PART.INDICATOR,
                    )
                    lv_obj.set_style_line_width(
                        scale_var,
                        await size.process(major[CONF_WIDTH]),
                        LV_PART.INDICATOR,
                    )
                    lv_obj.set_style_line_color(
                        scale_var,
                        await lv_color.process(major[CONF_COLOR]),
                        LV_PART.INDICATOR,
                    )

                    # Set label gap (padding)
                    label_gap = await size.process(major[CONF_LABEL_GAP])
                    if isinstance(label_gap, int):
                        label_gap -= DEFAULT_LABEL_GAP
                    lv_obj.set_style_pad_radial(
                        scale_var,
                        label_gap,
                        LV_PART.INDICATOR,
                    )
                else:
                    lv.scale_set_major_tick_every(scale_var, 0)
            else:
                lv.scale_set_total_tick_count(scale_var, 0)

            # Handle indicators as sections
            for indicator in scale_conf.get(CONF_INDICATORS, ()):
                (t, v) = next(iter(indicator.items()))
                iid = v[CONF_ID]

                # Enable getting the meter to which this belongs.

                # Set section range based on indicator values
                start_value = await get_start_value(v) or scale_conf[CONF_RANGE_FROM]
                end_value = await get_end_value(v) or scale_conf[CONF_RANGE_TO]

                # Create and apply styles based on indicator type
                if t == CONF_TICK_STYLE:
                    # No object created for this
                    color_start = await lv_color.process(v[CONF_COLOR_START])
                    color_end = await lv_color.process(v[CONF_COLOR_END])
                    local = v[CONF_LOCAL]
                    if color_start and color_end:
                        async with LambdaContext(
                            [(lv_event_t.operator("ptr"), "e")]
                        ) as lambda_:
                            lv.scale_draw_event_cb(
                                lambda_.get_parameter(0),
                                start_value,
                                end_value,
                                color_start,
                                color_end,
                                local,
                            )
                        lv_obj.add_event_cb(
                            scale_var,
                            await lambda_.get_lambda(),
                            LV_EVENT.DRAW_TASK_ADDED,
                            nullptr,
                        )
                        lv.obj_add_flag(scale_var, LV_OBJ_FLAG.SEND_DRAW_TASK_EVENTS)

                if t == CONF_ARC:
                    add_lv_use(CONF_ARC)
                    props = {
                        "arc_width": v[CONF_WIDTH],
                        "arc_color": v[CONF_COLOR],
                        "arc_opa": v[CONF_OPA],
                        "id": iid,
                        CONF_ALIGN: CHILD_ALIGNMENTS.CENTER,
                    }
                    if pad_all := v.get(CONF_PADDING, v.get(CONF_R_MOD, 0)):
                        props["pad_all"] = pad_all
                    lw = await widget_to_code(props, arc_indicator_type, scale_var)
                    await set_indicator_values(lw, v)

                if t == CONF_LINE:
                    add_lv_use(CONF_LINE)
                    # Needle represented by a line
                    if CONF_LENGTH in v:
                        length = v[CONF_LENGTH]
                    elif r_mod := v.get(CONF_R_MOD):
                        get_remapped_uses().add(CONF_R_MOD)
                        length = -abs(r_mod)
                    else:
                        length = 1.0
                    props = {
                        CONF_ID: v[CONF_ID],
                        CONF_OPA: v[CONF_OPA],
                        CONF_LINE_WIDTH: v[CONF_WIDTH],
                        "line_color": v[CONF_COLOR],
                        "line_rounded": True,
                        CONF_ALIGN: CHILD_ALIGNMENTS.TOP_LEFT,
                        CONF_LENGTH: length,
                        CONF_RADIAL_OFFSET: v[CONF_RADIAL_OFFSET],
                    }
                    lw = await widget_to_code(props, line_indicator_type, scale_var)
                    await set_indicator_values(lw, v)

                # Note: Image indicators (needles) are not directly supported by scale widget
                # They would need to be implemented as separate image objects positioned over the scale
                if t == CONF_IMAGE:
                    add_lv_use(CONF_IMAGE)
                    src = v[CONF_SRC]
                    src_data = CORE.data[IMAGE_DOMAIN][str(src)]
                    pivot_x = await pixels.process(v[CONF_PIVOT_X])
                    pivot_y = await pixels.process(
                        v.get(CONF_PIVOT_Y, src_data[CONF_HEIGHT] // 2)
                    )
                    props = {
                        CONF_X: src_data[CONF_WIDTH] // 2 - pivot_x,
                        "transform_pivot_x": pivot_x,
                        "transform_pivot_y": pivot_y,
                        CONF_SRC: src,
                        CONF_OPA: v[CONF_OPA],
                        CONF_ID: v[CONF_ID],
                        CONF_ALIGN: CHILD_ALIGNMENTS.CENTER,
                    }
                    iw = await widget_to_code(props, image_indicator_type, scale_var)
                    await set_indicator_values(iw, v)

        # Add a pivot
        # Get the default style
        pivot_style = PIVOT_STYLE.copy()
        pivot_style.update(config.get(CONF_INDICATOR, config.get(CONF_PIVOT, {})))
        with LocalVariable("pivot", lv_obj_t, lv_expr.container_create(var)) as pivot:
            pw = Widget(pivot, obj_spec, pivot_style)
            await set_obj_properties(pw, pivot_style)


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
        await set_indicator_values(w, config)

    return await action_to_code(
        widget, set_value, action_id, template_arg, args, config
    )


async def set_indicator_values(indicator: Widget, config):
    """Update scale section values (replaces meter indicator values)"""
    start_value = await get_start_value(config)
    end_value = await get_end_value(config)
    if indicator.type is scale_spec:
        # For scale sections, we update the range
        if start_value is not None and end_value is not None:
            lv.scale_section_set_range(indicator.obj, start_value, end_value)
        elif start_value is not None:
            # If only start value, use it as both start and end (single point)
            lv.scale_section_set_range(indicator.obj, start_value, start_value)
        elif end_value is not None:
            # If only end value, assume range from 0 to end_value
            lv.scale_section_set_range(indicator.obj, 0, end_value)
        return

    if indicator.type is arc_indicator_type:
        if start_value is not None:
            lv.arc_set_start_angle(
                indicator.obj, lv.get_needle_angle_for_value(indicator.obj, start_value)
            )
        if end_value is not None:
            lv.arc_set_end_angle(
                indicator.obj, lv.get_needle_angle_for_value(indicator.obj, start_value)
            )

    if start_value is None:
        return
    if indicator.type is line_indicator_type:
        # Line needle
        lv_add(indicator.var.set_value(start_value))
        return
    if indicator.type is image_indicator_type:
        # Needle represented by an image
        lv.image_set_needle_value(indicator.obj, start_value)
        lv_obj.set_style_transform_rotation(
            indicator.obj,
            lv.get_needle_angle_for_value(indicator.obj, start_value) * 10,
            LV_PART.MAIN,
        )
