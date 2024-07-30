from esphome import config_validation as cv
from esphome.const import CONF_ARGS, CONF_FORMAT, CONF_ID, CONF_STATE, CONF_TYPE
from esphome.schema_extractors import SCHEMA_EXTRACT

from . import defines as df, lv_validation as lvalid, types as ty
from .helpers import add_lv_use, requires_component, validate_printf
from .lv_validation import lv_font
from .types import WIDGET_TYPES, WidgetType

# A schema for text properties
TEXT_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_TEXT): cv.Any(
            cv.All(
                cv.Schema(
                    {
                        cv.Required(CONF_FORMAT): cv.string,
                        cv.Optional(CONF_ARGS, default=list): cv.ensure_list(
                            cv.lambda_
                        ),
                    },
                ),
                validate_printf,
            ),
            lvalid.lv_text,
        )
    }
)

# All LVGL styles and their validators
STYLE_PROPS = {
    "align": df.CHILD_ALIGNMENTS.one_of,
    "arc_opa": lvalid.opacity,
    "arc_color": lvalid.lv_color,
    "arc_rounded": lvalid.lv_bool,
    "arc_width": cv.positive_int,
    "anim_time": lvalid.lv_milliseconds,
    "bg_color": lvalid.lv_color,
    "bg_grad_color": lvalid.lv_color,
    "bg_dither_mode": df.LvConstant("LV_DITHER_", "NONE", "ORDERED", "ERR_DIFF").one_of,
    "bg_grad_dir": df.LvConstant("LV_GRAD_DIR_", "NONE", "HOR", "VER").one_of,
    "bg_grad_stop": lvalid.stop_value,
    "bg_image_opa": lvalid.opacity,
    "bg_image_recolor": lvalid.lv_color,
    "bg_image_recolor_opa": lvalid.opacity,
    "bg_main_stop": lvalid.stop_value,
    "bg_opa": lvalid.opacity,
    "border_color": lvalid.lv_color,
    "border_opa": lvalid.opacity,
    "border_post": lvalid.lv_bool,
    "border_side": df.LvConstant(
        "LV_BORDER_SIDE_", "NONE", "TOP", "BOTTOM", "LEFT", "RIGHT", "INTERNAL"
    ).several_of,
    "border_width": cv.positive_int,
    "clip_corner": lvalid.lv_bool,
    "height": lvalid.size,
    "image_recolor": lvalid.lv_color,
    "image_recolor_opa": lvalid.opacity,
    "line_width": cv.positive_int,
    "line_dash_width": cv.positive_int,
    "line_dash_gap": cv.positive_int,
    "line_rounded": lvalid.lv_bool,
    "line_color": lvalid.lv_color,
    "opa": lvalid.opacity,
    "opa_layered": lvalid.opacity,
    "outline_color": lvalid.lv_color,
    "outline_opa": lvalid.opacity,
    "outline_pad": lvalid.size,
    "outline_width": lvalid.size,
    "pad_all": lvalid.size,
    "pad_bottom": lvalid.size,
    "pad_column": lvalid.size,
    "pad_left": lvalid.size,
    "pad_right": lvalid.size,
    "pad_row": lvalid.size,
    "pad_top": lvalid.size,
    "shadow_color": lvalid.lv_color,
    "shadow_ofs_x": cv.int_,
    "shadow_ofs_y": cv.int_,
    "shadow_opa": lvalid.opacity,
    "shadow_spread": cv.int_,
    "shadow_width": cv.positive_int,
    "text_align": df.LvConstant(
        "LV_TEXT_ALIGN_", "LEFT", "CENTER", "RIGHT", "AUTO"
    ).one_of,
    "text_color": lvalid.lv_color,
    "text_decor": df.LvConstant(
        "LV_TEXT_DECOR_", "NONE", "UNDERLINE", "STRIKETHROUGH"
    ).several_of,
    "text_font": lv_font,
    "text_letter_space": cv.positive_int,
    "text_line_space": cv.positive_int,
    "text_opa": lvalid.opacity,
    "transform_angle": lvalid.angle,
    "transform_height": lvalid.pixels_or_percent,
    "transform_pivot_x": lvalid.pixels_or_percent,
    "transform_pivot_y": lvalid.pixels_or_percent,
    "transform_zoom": lvalid.zoom,
    "translate_x": lvalid.pixels_or_percent,
    "translate_y": lvalid.pixels_or_percent,
    "max_height": lvalid.pixels_or_percent,
    "max_width": lvalid.pixels_or_percent,
    "min_height": lvalid.pixels_or_percent,
    "min_width": lvalid.pixels_or_percent,
    "radius": lvalid.radius,
    "width": lvalid.size,
    "x": lvalid.pixels_or_percent,
    "y": lvalid.pixels_or_percent,
}

STYLE_REMAP = {
    "bg_image_opa": "bg_img_opa",
    "bg_image_recolor": "bg_img_recolor",
    "bg_image_recolor_opa": "bg_img_recolor_opa",
    "bg_image_src": "bg_img_src",
    "image_recolor": "img_recolor",
    "image_recolor_opa": "img_recolor_opa",
}

# Complete object style schema
STYLE_SCHEMA = cv.Schema({cv.Optional(k): v for k, v in STYLE_PROPS.items()}).extend(
    {
        cv.Optional(df.CONF_SCROLLBAR_MODE): df.LvConstant(
            "LV_SCROLLBAR_MODE_", "OFF", "ON", "ACTIVE", "AUTO"
        ).one_of,
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
FLAG_LIST = cv.ensure_list(df.LvConstant("LV_OBJ_FLAG_", *df.OBJ_FLAGS).one_of)


def part_schema(widget_type: WidgetType):
    """
    Generate a schema for the various parts (e.g. main:, indicator:) of a widget type
    :param widget_type:  The type of widget to generate for
    :return:
    """
    parts = widget_type.parts
    return cv.Schema({cv.Optional(part): STATE_SCHEMA for part in parts}).extend(
        STATE_SCHEMA
    )


def obj_schema(widget_type: WidgetType):
    """
    Create a schema for a widget type itself i.e. no allowance for children
    :param widget_type:
    :return:
    """
    return (
        part_schema(widget_type)
        .extend(FLAG_SCHEMA)
        .extend(ALIGN_TO_SCHEMA)
        .extend(
            cv.Schema(
                {
                    cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
                }
            )
        )
    )


ALIGN_TO_SCHEMA = {
    cv.Optional(df.CONF_ALIGN_TO): cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_obj_t),
            cv.Required(df.CONF_ALIGN): df.ALIGN_ALIGNMENTS.one_of,
            cv.Optional(df.CONF_X, default=0): lvalid.pixels_or_percent,
            cv.Optional(df.CONF_Y, default=0): lvalid.pixels_or_percent,
        }
    )
}


# A style schema that can include text
STYLED_TEXT_SCHEMA = cv.maybe_simple_value(
    STYLE_SCHEMA.extend(TEXT_SCHEMA), key=df.CONF_TEXT
)

ALL_STYLES = {
    **STYLE_PROPS,
}


def container_validator(schema, widget_type: WidgetType):
    """
    Create a validator for a container given the widget type
    :param schema: Base schema to extend
    :param widget_type:
    :return:
    """

    def validator(value):
        result = schema
        if w_sch := widget_type.schema:
            result = result.extend(w_sch)
        if value and (layout := value.get(df.CONF_LAYOUT)):
            if not isinstance(layout, dict):
                raise cv.Invalid("Layout value must be a dict")
            ltype = layout.get(CONF_TYPE)
            add_lv_use(ltype)
        result = result.extend(
            {cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema())}
        )
        if value == SCHEMA_EXTRACT:
            return result
        return result(value)

    return validator


def container_schema(widget_type: WidgetType, extras=None):
    """
    Create a schema for a container widget of a given type. All obj properties are available, plus
    the extras passed in, plus any defined for the specific widget being specified.
    :param widget_type:     The widget type, e.g. "img"
    :param extras:  Additional options to be made available, e.g. layout properties for children
    :return: The schema for this type of widget.
    """
    schema = obj_schema(widget_type).extend(
        {cv.GenerateID(): cv.declare_id(widget_type.w_type)}
    )
    if extras:
        schema = schema.extend(extras)
    # Delayed evaluation for recursion
    return container_validator(schema, widget_type)


def widget_schema(widget_type: WidgetType, extras=None):
    """
    Create a schema for a given widget type
    :param widget_type: The name of the widget
    :param extras:
    :return:
    """
    validator = container_schema(widget_type, extras=extras)
    if required := widget_type.required_component:
        validator = cv.All(validator, requires_component(required))
    return cv.Exclusive(widget_type.name, df.CONF_WIDGETS), validator


# All widget schemas must be defined before this is called.


def any_widget_schema(extras=None):
    """
    Generate schemas for all possible LVGL widgets. This is what implements the ability to have a list of any kind of
    widget under the widgets: key.

    :param extras: Additional schema to be applied to each generated one
    :return:
    """
    return cv.Any(dict(widget_schema(wt, extras) for wt in WIDGET_TYPES.values()))
