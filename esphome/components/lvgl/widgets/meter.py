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
from esphome.cpp_generator import MockObj

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
    LV_PART,
    literal, LV_SCALE_MODE,
)
from ..helpers import lvgl_components_required
from ..lv_validation import (
    get_end_value,
    get_start_value,
    lv_angle,
    lv_angle_degrees,
    lv_bool,
    lv_color,
    lv_float,
    lv_image,
    lv_int,
    opacity,
    pixels,
    requires_component,
    size, lv_pct,
)
from ..lvcode import LocalVariable, lv, lv_expr, lv_obj
from ..styles import create_style
from ..types import ObjUpdateAction, lv_obj_t
from . import Widget, WidgetType, get_widgets
from .arc import CONF_ARC
from .img import CONF_IMAGE
from .line import CONF_LINE
from .scale import lv_scale_section_t, section_spec, lv_scale_t

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
# Limitations in this emulation:
# - Image needles are not directly supported (would need separate image widgets)
# - Some advanced meter features may not be available
# - Gradient colors on tick styles are simplified to single colors


# For compatibility, keep meter types but map to scale
lv_meter_t = lv_obj_t
lv_meter_indicator_t = lv_scale_section_t


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
        cv.Optional(CONF_RANGE_FROM, default=0.0): lv_int,
        cv.Optional(CONF_RANGE_TO, default=100.0): lv_int,
        cv.Optional(CONF_ANGLE_RANGE, default=270): lv_angle_degrees,
        cv.Optional(CONF_ROTATION): lv_angle_degrees,
        cv.Optional(CONF_INDICATORS): cv.ensure_list(INDICATOR_SCHEMA),
    }
)

METER_SCHEMA = {cv.Optional(CONF_SCALES): cv.ensure_list(SCALE_SCHEMA)}


class MeterType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_METER,
            lv_obj_t,  # Use scale widget instead of meter
            # Note that mapping from 8.x to 9.x, indicator styling is applied to needles, and tick styling
            # is migrated to indicator
            (CONF_MAIN, CONF_INDICATOR, CONF_TICKS, CONF_ITEMS),
            METER_SCHEMA,
            lv_name="obj",
        )

    def on_create(self, var: MockObj, config: dict):
        # Remove theme styling from outer container
        lv.obj_remove_style_all(var)

    def validate(self, value):
        value = value.copy()
        if indicator_style := value.get(CONF_INDICATOR):
            # value[CONF_INDICATORS] = indicator_style
            del value[CONF_INDICATOR]
        if ticks := value.get(CONF_TICKS):
            value[CONF_INDICATOR] = ticks
            del value[CONF_TICKS]
        return value

    async def to_code(self, w: Widget, config):
        """For a meter object using scale widget, create and set parameters"""

        lvgl_components_required.add("scale")  # Use scale component
        var = w.obj

        # LVGL 9.4 scale widget setup
        # Set to round mode for meter-like appearance
        # Background style will be applied
        lv_obj.set_style_radius(var, literal("LV_RADIUS_CIRCLE"), 0)
        # Add a pivot
        with LocalVariable("pivot", lv_obj_t, lv_expr.obj_create(var)) as pivot:
            lv.obj_remove_style_all(pivot)
            lv_obj.set_style_radius(pivot, literal("LV_RADIUS_CIRCLE"), LV_PART.MAIN)
            lv_obj.set_style_align(pivot, literal("LV_ALIGN_CENTER"), LV_PART.MAIN)
            lv_obj.set_style_bg_color(pivot, lv_color.black, LV_PART.MAIN)
            lv_obj.set_style_bg_opa(pivot, literal("LV_OPA_COVER"), LV_PART.MAIN)
            lv_obj.set_style_width(pivot, 15, 0)
            lv_obj.set_style_height(pivot, 15, 0)

        for scale_conf in config.get(CONF_SCALES, ()):
            print(scale_conf)
            with LocalVariable("scale", lv_obj_t, lv_expr.scale_create(var)) as scale_var:
                lv_obj.set_style_height(scale_var, lv_pct(100), LV_PART.MAIN)
                lv_obj.set_style_width(scale_var, lv_pct(100), LV_PART.MAIN)
                lv_obj.set_style_align(scale_var, literal("LV_ALIGN_CENTER"), LV_PART.MAIN)
                lv_obj.set_style_bg_opa(scale_var, literal("LV_OPA_TRANSP"), LV_PART.MAIN)
                lv_obj.set_style_radius(scale_var, literal("LV_RADIUS_CIRCLE"), 0)
                lv.scale_set_mode(scale_var, LV_SCALE_MODE.ROUND_INNER)
                # Set the scale range
                lv.scale_set_range(
                    scale_var,
                    await lv_int.process(scale_conf[CONF_RANGE_FROM]),
                    await lv_int.process(scale_conf[CONF_RANGE_TO]),
                )

                # Set angle range
                lv.scale_set_angle_range(
                    scale_var, await lv_angle_degrees.process(scale_conf[CONF_ANGLE_RANGE])
                )

                # Set rotation if specified
                if CONF_ROTATION in scale_conf:
                    rotation = await lv_angle_degrees.process(scale_conf[CONF_ROTATION])
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
                    lv_obj.set_style_line_color(
                        scale_var, await lv_color.process(ticks[CONF_COLOR]), LV_PART.ITEMS
                    )

                    if CONF_MAJOR in ticks:
                        major = ticks[CONF_MAJOR]
                        # Set major tick frequency
                        lv.scale_set_major_tick_every(scale_var, major[CONF_STRIDE])

                        # Enable labels for major ticks
                        lv.scale_set_label_show(scale_var, True)

                        # Set major tick styling
                        lv_obj.set_style_length(
                            scale_var, await size.process(major[CONF_LENGTH]), LV_PART.INDICATOR
                        )
                        lv_obj.set_style_line_width(
                            scale_var, await size.process(major[CONF_WIDTH]), LV_PART.INDICATOR
                        )
                        lv_obj.set_style_line_color(
                            scale_var,
                            await lv_color.process(major[CONF_COLOR]),
                            LV_PART.INDICATOR,
                        )

                        # Set label gap (padding)
                        lv_obj.set_style_pad_radial(
                            scale_var,
                            await size.process(major[CONF_LABEL_GAP]),
                            LV_PART.INDICATOR,
                        )

                # Handle indicators as sections
                for indicator in scale_conf.get(CONF_INDICATORS, ()):
                    (t, v) = next(iter(indicator.items()))
                    iid = v[CONF_ID]

                    # Create a section for this indicator
                    section_var = cg.Pvariable(iid, lv_expr.scale_add_section(w.obj))

                    # Enable getting the meter to which this belongs.
                    section_widget = Widget.create(iid, var, section_spec, v)
                    section_widget.obj = section_var

                    # Set section range based on indicator values
                    start_value = await get_start_value(v) or scale_conf[CONF_RANGE_FROM]
                    end_value = await get_end_value(v) or scale_conf[CONF_RANGE_TO]

                    lv.scale_section_set_range(section_var, start_value, end_value)

                    # Create and apply styles based on indicator type
                    style_var = await create_style(f"{iid}_style_")
                    if t == CONF_LINE:
                        # For line indicators, style the main line
                        lv.style_set_line_color(
                            style_var, await lv_color.process(v[CONF_COLOR])
                        )
                        lv.style_set_line_width(
                            style_var, await size.process(v[CONF_WIDTH])
                        )
                        lv.scale_section_set_style(section_var, LV_PART.MAIN, style_var)

                    elif t == CONF_ARC:
                        # For arc indicators, style the main arc
                        lv.style_set_arc_color(
                            style_var, await lv_color.process(v[CONF_COLOR])
                        )
                        lv.style_set_arc_width(style_var, await size.process(v[CONF_WIDTH]))
                        lv.scale_section_set_style(section_var, LV_PART.MAIN, style_var)

                    elif t == CONF_TICK_STYLE:
                        color_start = await lv_color.process(v[CONF_COLOR_START])
                        # Note: LVGL 9.4 scale doesn't support gradient colors on ticks
                        # Use start color for now
                        lv.style_set_line_color(style_var, color_start)
                        lv.style_set_line_width(
                            style_var, await size.process(v[CONF_WIDTH])
                        )
                        lv.scale_section_set_style(section_var, LV_PART.ITEMS, style_var)

                    # Note: Image indicators (needles) are not directly supported by scale widget
                    # They would need to be implemented as separate image objects positioned over the scale
                    if t == CONF_IMAGE:
                        # This would require creating a separate image widget and positioning it
                        # For now, we'll skip this or could implement as overlay
                        pass


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

    return await action_to_code(
        widget, set_value, action_id, template_arg, args, config
    )


async def set_indicator_values(scale, section, config):
    """Update scale section values (replaces meter indicator values)"""
    start_value = await get_start_value(config)
    end_value = await get_end_value(config)

    # For scale sections, we update the range
    if start_value is not None and end_value is not None:
        lv.scale_section_set_range(section, start_value, end_value)
    elif start_value is not None:
        # If only start value, use it as both start and end (single point)
        lv.scale_section_set_range(section, start_value, start_value)
    elif end_value is not None:
        # If only end value, assume range from 0 to end_value
        lv.scale_section_set_range(section, 0, end_value)

    # Note: Opacity for sections would need to be handled through style properties
    # This is more complex in LVGL 9.4 scale sections
    if (opa := config.get(CONF_OPA)) is not None:
        # Would need to create/update section style with opacity
        # For now, we'll skip this as it requires more complex style management
        pass

    lv_obj.invalidate(scale)
