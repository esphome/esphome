from collections.abc import Callable

from esphome import config_validation as cv
from esphome.automation import Trigger, validate_automation
from esphome.components.time import RealTimeClock
from esphome.config_validation import prepend_path
from esphome.const import (
    CONF_ARGS,
    CONF_FORMAT,
    CONF_GROUP,
    CONF_ID,
    CONF_ON_BOOT,
    CONF_ON_VALUE,
    CONF_STATE,
    CONF_TEXT,
    CONF_TIME,
    CONF_TRIGGER_ID,
    CONF_X,
    CONF_Y,
)
from esphome.core import TimePeriod
from esphome.core.config import StartupTrigger

from . import defines as df, lv_validation as lvalid
from .defines import (
    CONF_EXT_CLICK_AREA,
    CONF_SCROLL_DIR,
    CONF_SCROLL_SNAP_X,
    CONF_SCROLL_SNAP_Y,
    CONF_SCROLLBAR_MODE,
    CONF_TIME_FORMAT,
    LV_GRAD_DIR,
    get_remapped_uses,
)
from .helpers import CONF_IF_NAN, requires_component, validate_printf
from .layout import (
    FLEX_OBJ_SCHEMA,
    GRID_CELL_SCHEMA,
    append_layout_schema,
    grid_alignments,
)
from .lv_validation import lv_color, lv_font, lv_gradient, lv_image, opacity
from .lvcode import LvglComponent, lv_event_t_ptr
from .types import (
    LVEncoderListener,
    LvType,
    lv_group_t,
    lv_obj_t,
    lv_pseudo_button_t,
    lv_style_t,
)
from .widgets import WidgetType

# this will be populated later, in __init__.py to avoid circular imports.
WIDGET_TYPES: dict = {}


TIME_TEXT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TIME_FORMAT): cv.string,
        cv.GenerateID(CONF_TIME): cv.templatable(cv.use_id(RealTimeClock)),
    }
)

PRINTF_TEXT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_FORMAT): cv.string,
            cv.Optional(CONF_ARGS, default=list): cv.ensure_list(cv.lambda_),
            cv.Optional(CONF_IF_NAN): cv.string,
        },
    ),
    validate_printf,
)


def _validate_text(value):
    """
    Do some sanity checking of the format to get better error messages
    than using cv.Any
    """
    if value is None:
        raise cv.Invalid("No text specified")
    if isinstance(value, dict):
        if CONF_TIME_FORMAT in value:
            return TIME_TEXT_SCHEMA(value)
        return PRINTF_TEXT_SCHEMA(value)

    return cv.templatable(cv.string)(value)


# A schema for text properties
TEXT_SCHEMA = {
    cv.Optional(CONF_TEXT): _validate_text,
}

LIST_ACTION_SCHEMA = cv.ensure_list(
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_pseudo_button_t),
        },
        key=CONF_ID,
    )
)

PRESS_TIME = cv.All(
    lvalid.lv_milliseconds, cv.Range(max=TimePeriod(milliseconds=65535))
)

ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.All(
            cv.declare_id(LVEncoderListener), requires_component("binary_sensor")
        ),
        cv.Optional(CONF_GROUP): cv.declare_id(lv_group_t),
        cv.Optional(df.CONF_INITIAL_FOCUS): cv.All(
            LIST_ACTION_SCHEMA, cv.Length(min=1, max=1)
        ),
        cv.Optional(df.CONF_LONG_PRESS_TIME, default="400ms"): PRESS_TIME,
        cv.Optional(df.CONF_LONG_PRESS_REPEAT_TIME, default="100ms"): PRESS_TIME,
    }
)

POINT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_X): lvalid.pixels_or_percent,
        cv.Required(CONF_Y): lvalid.pixels_or_percent,
    }
)


def point_schema(value):
    """
    A shorthand for a point in the form of x,y
    :param value: The value to check
    :return: The value as a tuple of x,y
    """
    if isinstance(value, dict):
        return POINT_SCHEMA(value)
    if isinstance(value, list):
        if len(value) != 2:
            raise cv.Invalid("Invalid point format, should be <x_int>, <y_int>")
        return POINT_SCHEMA({CONF_X: value[0], CONF_Y: value[1]})
    try:
        x, y = str(value).split(",")
        return POINT_SCHEMA({CONF_X: x, CONF_Y: y})
    except ValueError:
        pass
    # not raising this in the catch block because pylint doesn't like it
    raise cv.Invalid("Invalid point format, should be <x_int>, <y_int>")


# All LVGL styles and their validators
BASE_PROPS = {
    "align": df.CHILD_ALIGNMENTS.one_of,
    "anim_duration": lvalid.lv_milliseconds,
    "arc_color": lvalid.lv_color,
    "arc_opa": lvalid.opacity,
    "arc_rounded": lvalid.lv_bool,
    "arc_width": lvalid.pixels,
    "base_dir": df.LvConstant("LV_BASE_DIR_", "LTR", "RTL", "AUTO").one_of,
    "bg_color": lvalid.lv_color,
    "bg_grad": lv_gradient,
    "bg_grad_color": lvalid.lv_color,
    "bg_grad_dir": LV_GRAD_DIR.one_of,
    "bg_grad_opa": lvalid.opacity,
    "bg_grad_stop": lvalid.stop_value,
    "bg_image_opa": lvalid.opacity,
    "bg_image_recolor": lvalid.lv_color,
    "bg_image_recolor_opa": lvalid.opacity,
    "bg_image_src": lvalid.lv_image,
    "bg_image_tiled": lvalid.lv_bool,
    "bg_main_opa": lvalid.opacity,
    "bg_main_stop": lvalid.stop_value,
    "bg_opa": lvalid.opacity,
    "bitmap_mask_src": lvalid.lv_image,
    "blend_mode": df.LvConstant(
        "LV_BLEND_MODE_",
        "NORMAL",
        "ADDITIVE",
        "SUBTRACTIVE",
        "MULTIPLY",
        "DIFFERENCE",
    ).one_of,
    "blur_backdrop": lvalid.lv_bool,
    "blur_quality": df.LvConstant(
        "LV_BLUR_QUALITY_", "AUTO", "SPEED", "PRECISION"
    ).one_of,
    "blur_radius": lvalid.lv_positive_int,
    "border_color": lvalid.lv_color,
    "border_opa": lvalid.opacity,
    "border_post": lvalid.lv_bool,
    "border_side": df.LvConstant(
        "LV_BORDER_SIDE_", "NONE", "TOP", "BOTTOM", "LEFT", "RIGHT", "INTERNAL"
    ).several_of,
    "border_width": lvalid.lv_positive_int,
    "clip_corner": lvalid.lv_bool,
    "color_filter_opa": lvalid.opacity,
    "drop_shadow_color": lvalid.lv_color,
    "drop_shadow_offset_x": lvalid.lv_int,
    "drop_shadow_offset_y": lvalid.lv_int,
    "drop_shadow_opa": lvalid.opacity,
    "drop_shadow_quality": df.LvConstant(
        "LV_BLUR_QUALITY_", "AUTO", "SPEED", "PRECISION"
    ).one_of,
    "drop_shadow_radius": lvalid.lv_positive_int,
    "height": lvalid.size,
    "image_opa": lvalid.opacity,
    "image_recolor": lvalid.lv_color,
    "image_recolor_opa": lvalid.opacity,
    "length": lvalid.pixels_or_percent,
    "line_color": lvalid.lv_color,
    "line_dash_gap": lvalid.lv_positive_int,
    "line_dash_width": lvalid.lv_positive_int,
    "line_opa": lvalid.opacity,
    "line_rounded": lvalid.lv_bool,
    "line_width": lvalid.lv_positive_int,
    "margin_bottom": lvalid.padding,
    "margin_left": lvalid.padding,
    "margin_right": lvalid.padding,
    "margin_top": lvalid.padding,
    "max_height": lvalid.pixels_or_percent,
    "max_width": lvalid.pixels_or_percent,
    "min_height": lvalid.pixels_or_percent,
    "min_width": lvalid.pixels_or_percent,
    "opa": lvalid.opacity,
    "opa_layered": lvalid.opacity,
    "outline_color": lvalid.lv_color,
    "outline_opa": lvalid.opacity,
    "outline_pad": lvalid.padding,
    "outline_width": lvalid.pixels,
    "pad_all": lvalid.padding,
    "pad_bottom": lvalid.padding,
    "pad_left": lvalid.padding,
    "pad_radial": lvalid.padding,
    "pad_right": lvalid.padding,
    "pad_top": lvalid.padding,
    "radial_offset": lvalid.size,
    "radius": lvalid.lv_fraction,
    "recolor": lvalid.lv_color,
    "recolor_opa": lvalid.opacity,
    "rotary_sensitivity": lvalid.lv_positive_int,
    "shadow_color": lvalid.lv_color,
    "shadow_offset_x": lvalid.lv_int,
    "shadow_offset_y": lvalid.lv_int,
    "shadow_opa": lvalid.opacity,
    "shadow_spread": lvalid.lv_int,
    "shadow_width": lvalid.lv_positive_int,
    "text_align": df.LvConstant(
        "LV_TEXT_ALIGN_", "LEFT", "CENTER", "RIGHT", "AUTO"
    ).one_of,
    "text_color": lvalid.lv_color,
    "text_decor": df.LvConstant(
        "LV_TEXT_DECOR_", "NONE", "UNDERLINE", "STRIKETHROUGH"
    ).several_of,
    "text_font": lv_font,
    "text_letter_space": lvalid.lv_positive_int,
    "text_line_space": lvalid.lv_positive_int,
    "text_opa": lvalid.opacity,
    "text_outline_stroke_color": lvalid.lv_color,
    "text_outline_stroke_opa": lvalid.opacity,
    "text_outline_stroke_width": lvalid.lv_positive_int,
    "transform_height": lvalid.pixels_or_percent,
    "transform_pivot_x": lvalid.pixels_or_percent,
    "transform_pivot_y": lvalid.pixels_or_percent,
    "transform_rotation": lvalid.lv_angle,
    "transform_scale": lvalid.scale,
    "transform_scale_x": lvalid.scale,
    "transform_scale_y": lvalid.scale,
    "transform_skew_x": lvalid.lv_angle,
    "transform_skew_y": lvalid.lv_angle,
    "transform_width": lvalid.pixels_or_percent,
    "translate_radial": lvalid.lv_int,
    "translate_x": lvalid.pixels_or_percent,
    "translate_y": lvalid.pixels_or_percent,
    "width": lvalid.size,
    "x": lvalid.pixels_or_percent,
    "y": lvalid.pixels_or_percent,
}

STYLE_REMAP = {
    "anim_time": "anim_duration",
    "transform_angle": "transform_rotation",
    "transform_zoom": "transform_scale",
    "zoom": "scale",
    "angle": "rotation",
    "shadow_ofs_x": "shadow_offset_x",
    "shadow_ofs_y": "shadow_offset_y",
    "r_mod": "length",
}

STYLE_PROPS = BASE_PROPS | {
    p: BASE_PROPS[v] for p, v in STYLE_REMAP.items() if v in BASE_PROPS
}


def remap_property(prop, record=True):
    """
    Remap an old style property to new style property.
    Optionally record the use of the deprecated property.
    :param prop: Name of the style property to remap.
    :param record: Whether to record the use of the deprecated property.
    :return: The remapped property name, or ``prop`` if no remapping exists.
    """
    if prop in STYLE_REMAP:
        if record:
            get_remapped_uses().add(prop)
        return STYLE_REMAP[prop]
    return prop


# Complete object style schema
STYLE_SCHEMA = cv.Schema({cv.Optional(k): v for k, v in STYLE_PROPS.items()}).extend(
    {
        cv.Optional(df.CONF_STYLES): cv.ensure_list(cv.use_id(lv_style_t)),
        cv.Optional(df.CONF_SCROLLBAR_MODE): df.LvConstant(
            "LV_SCROLLBAR_MODE_", "OFF", "ON", "ACTIVE", "AUTO"
        ).one_of,
        cv.Optional(CONF_EXT_CLICK_AREA): lvalid.pixels,
        cv.Optional(CONF_SCROLL_DIR): df.SCROLL_DIRECTIONS.one_of,
        cv.Optional(CONF_SCROLL_SNAP_X): df.SNAP_DIRECTIONS.one_of,
        cv.Optional(CONF_SCROLL_SNAP_Y): df.SNAP_DIRECTIONS.one_of,
    }
)

OBJ_PROPERTIES = {
    CONF_EXT_CLICK_AREA,
    CONF_SCROLL_SNAP_X,
    CONF_SCROLL_SNAP_Y,
    CONF_SCROLL_DIR,
    CONF_SCROLLBAR_MODE,
}

# Also allow widget specific properties for use in style definitions
FULL_STYLE_SCHEMA = STYLE_SCHEMA.extend(
    {
        cv.Optional(df.CONF_GRID_CELL_X_ALIGN): grid_alignments,
        cv.Optional(df.CONF_GRID_CELL_Y_ALIGN): grid_alignments,
        cv.Optional(df.CONF_PAD_ROW): lvalid.padding,
        cv.Optional(df.CONF_PAD_COLUMN): lvalid.padding,
    }
)

# Object states. Top level properties apply to MAIN
STATE_SCHEMA = cv.Schema(
    {cv.Optional(state): STYLE_SCHEMA for state in df.STATES}
).extend(STYLE_SCHEMA)
# Setting object states
SET_STATE_SCHEMA = cv.Schema(
    {cv.Optional(state): lvalid.lv_bool for state in df.STATES}
)
# Setting object flags
FLAG_SCHEMA = cv.Schema({cv.Optional(flag): lvalid.lv_bool for flag in df.OBJ_FLAGS})
FLAG_LIST = cv.ensure_list(df.LV_OBJ_FLAG.one_of)


def part_schema(parts):
    """
    Generate a schema for the various parts (e.g. main:, indicator:) of a widget type
    :param parts:  The parts to include
    :return: The schema
    """
    return STATE_SCHEMA.extend(FLAG_SCHEMA).extend(
        {cv.Optional(part): STATE_SCHEMA for part in parts}
    )


def automation_schema(typ: LvType):
    events = df.LV_EVENT_TRIGGERS + df.SWIPE_TRIGGERS
    if typ.has_on_value:
        events = events + (CONF_ON_VALUE,)
    args = typ.get_arg_type()
    args.append(lv_event_t_ptr)
    return {
        **{
            cv.Optional(event): validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Trigger.template(*args)
                    ),
                }
            )
            for event in events
        },
        cv.Optional(CONF_ON_BOOT): validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StartupTrigger)}
        ),
    }


def _update_widget(widget_type: WidgetType) -> Callable[[dict], dict]:
    """
    During validation of update actions, create a map of action types to affected widgets
    for use in final validation.
    :param widget_type:
    :return:
    """

    def validator(value: dict) -> dict:
        df.get_data(df.KEY_UPDATED_WIDGETS).setdefault(widget_type, []).append(value)
        return value

    return validator


def base_update_schema(widget_type: WidgetType | LvType, parts):
    """
    Create a schema for updating a widget's style properties, states and flags.
    :param widget_type: The type of the ID
    :param parts:  The allowable parts to specify
    :return:
    """

    w_type = widget_type.w_type if isinstance(widget_type, WidgetType) else widget_type
    schema = part_schema(parts).extend(
        {
            cv.Required(CONF_ID): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_ID): cv.use_id(w_type),
                    },
                    key=CONF_ID,
                )
            ),
            cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
        }
    )

    if isinstance(widget_type, WidgetType):
        schema.add_extra(_update_widget(widget_type))
    return schema


def obj_schema(widget_type: WidgetType):
    """
    Create a schema for a widget type itself i.e. no allowance for children
    :param widget_type:
    :return:
    """
    return (
        part_schema(widget_type.parts)
        .extend(ALIGN_TO_SCHEMA)
        .extend(automation_schema(widget_type.w_type))
        .extend(
            {
                cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
                cv.Optional(CONF_GROUP): cv.use_id(lv_group_t),
            }
        )
    )


ALIGN_TO_SCHEMA = {
    cv.Optional(df.CONF_ALIGN_TO): cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_obj_t),
            cv.Required(df.CONF_ALIGN): df.ALIGN_ALIGNMENTS.one_of,
            cv.Optional(CONF_X, default=0): lvalid.pixels_or_percent,
            cv.Optional(CONF_Y, default=0): lvalid.pixels_or_percent,
        }
    )
}


DISP_BG_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_DISP_BG_IMAGE): cv.Any(
            cv.one_of("none", lower=True), lv_image
        ),
        cv.Optional(df.CONF_DISP_BG_COLOR): lv_color,
        cv.Optional(df.CONF_DISP_BG_OPA): opacity,
    }
)

# A style schema that can include text
STYLED_TEXT_SCHEMA = cv.maybe_simple_value(
    STYLE_SCHEMA.extend(TEXT_SCHEMA), key=CONF_TEXT
)

# For use by platform components
LVGL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(df.CONF_LVGL_ID): cv.use_id(LvglComponent),
    }
)

ALL_STYLES = {
    **STYLE_PROPS,
    **GRID_CELL_SCHEMA,
    **FLEX_OBJ_SCHEMA,
    cv.Optional(df.CONF_PAD_ROW): lvalid.padding,
    cv.Optional(df.CONF_PAD_COLUMN): lvalid.padding,
}


def strip_defaults(schema: cv.Schema):
    """
    Take a schema and remove any default values, also convert Required to Optional.
    Useful for converting an object schema to a modify schema
    :param schema: The original Schema
    :return: A new schema with no defaults and all items optional
    """

    return cv.Schema({cv.Optional(k): v for k, v in schema.schema.items()})


def container_schema(widget_type: WidgetType, extras=None):
    """
    Create a schema for a container widget of a given type. All obj properties are available, plus
    the extras passed in, plus any defined for the specific widget being specified.
    :param widget_type:     The widget type, e.g. "image"
    :param extras:  Additional options to be made available, e.g. layout properties for children
    :return: The schema for this type of widget.
    """
    schema = obj_schema(widget_type).extend(
        {cv.GenerateID(): cv.declare_id(widget_type.w_type)}
    )
    if extras:
        schema = schema.extend(extras)
    # Delayed evaluation for recursion

    schema = schema.extend(widget_type.schema)

    def validator(value):
        value = value or {}
        return append_layout_schema(schema, value)(value)

    return validator


def any_widget_schema(extras=None):
    """
    Dynamically generate schemas for all possible LVGL widgets. This is what implements the ability to have a list of any kind of
    widget under the widgets: key.

    This uses lazy evaluation - the schema is built when called during validation,
    not at import time. This allows external components to register widgets
    before schema validation begins.

    :param extras: Additional schema to be applied to each generated one
    :return: A validator for the Widgets key
    """

    def validator(value):
        if isinstance(value, dict):
            # Convert to list
            is_dict = True
            value = [{k: v} for k, v in value.items()]
        else:
            is_dict = False
        if not isinstance(value, list):
            raise cv.Invalid("Expected a list of widgets")
        result = []
        for index, entry in enumerate(value):
            if not isinstance(entry, dict) or len(entry) != 1:
                raise cv.Invalid(
                    "Each widget must be a dictionary with a single key", path=[index]
                )
            [(key, value)] = entry.items()
            # Validate the widget against its schema
            widget_type = WIDGET_TYPES.get(key)
            if not widget_type:
                raise cv.Invalid(f"Unknown widget type: {key}", path=[index])
            container_validator = container_schema(widget_type, extras=extras)
            if required := widget_type.required_component:
                container_validator = cv.All(
                    container_validator, requires_component(required)
                )
            # Apply custom validation
            path = [key] if is_dict else [index, key]
            with prepend_path(path):
                value = widget_type.validate(value or {})
                result.append({key: container_validator(value)})
        return result

    return validator
