from esphome import config_validation as cv
from esphome.automation import Trigger, validate_automation
from esphome.const import (
    CONF_ARGS,
    CONF_FORMAT,
    CONF_GROUP,
    CONF_ID,
    CONF_ON_VALUE,
    CONF_STATE,
    CONF_TRIGGER_ID,
    CONF_TYPE,
)
from esphome.schema_extractors import SCHEMA_EXTRACT

from . import defines as df, lv_validation as lv, types as ty
from .defines import WIDGET_PARTS
from .helpers import REQUIRED_COMPONENTS, add_lv_use, validate_printf
from .types import WIDGET_TYPES, get_widget_type, lv_obj_t

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
            lv.lv_text,
        )
    }
)

STYLE_PROPS = {
    "align": df.CHILD_ALIGNMENTS.one_of,
    "arc_opa": lv.opacity,
    "arc_color": lv.lv_color,
    "arc_rounded": lv.lv_bool,
    "arc_width": cv.positive_int,
    "anim_time": lv.lv_milliseconds,
    "bg_color": lv.lv_color,
    "bg_grad_color": lv.lv_color,
    "bg_dither_mode": df.LvConstant("LV_DITHER_", "NONE", "ORDERED", "ERR_DIFF").one_of,
    "bg_grad_dir": df.LvConstant("LV_GRAD_DIR_", "NONE", "HOR", "VER").one_of,
    "bg_grad_stop": lv.stop_value,
    "bg_img_opa": lv.opacity,
    "bg_img_recolor": lv.lv_color,
    "bg_img_recolor_opa": lv.opacity,
    "bg_main_stop": lv.stop_value,
    "bg_opa": lv.opacity,
    "border_color": lv.lv_color,
    "border_opa": lv.opacity,
    "border_post": lv.bool_,
    "border_side": df.LvConstant(
        "LV_BORDER_SIDE_", "NONE", "TOP", "BOTTOM", "LEFT", "RIGHT", "INTERNAL"
    ).several_of,
    "border_width": cv.positive_int,
    "clip_corner": lv.lv_bool,
    "height": lv.size,
    "img_recolor": lv.lv_color,
    "img_recolor_opa": lv.opacity,
    "line_width": cv.positive_int,
    "line_dash_width": cv.positive_int,
    "line_dash_gap": cv.positive_int,
    "line_rounded": lv.lv_bool,
    "line_color": lv.lv_color,
    "opa": lv.opacity,
    "opa_layered": lv.opacity,
    "outline_color": lv.lv_color,
    "outline_opa": lv.opacity,
    "outline_pad": lv.size,
    "outline_width": lv.size,
    "pad_all": lv.size,
    "pad_bottom": lv.size,
    "pad_column": lv.size,
    "pad_left": lv.size,
    "pad_right": lv.size,
    "pad_row": lv.size,
    "pad_top": lv.size,
    "shadow_color": lv.lv_color,
    "shadow_ofs_x": cv.int_,
    "shadow_ofs_y": cv.int_,
    "shadow_opa": lv.opacity,
    "shadow_spread": cv.int_,
    "shadow_width": cv.positive_int,
    "text_align": df.LvConstant(
        "LV_TEXT_ALIGN_", "LEFT", "CENTER", "RIGHT", "AUTO"
    ).one_of,
    "text_color": lv.lv_color,
    "text_decor": df.LvConstant(
        "LV_TEXT_DECOR_", "NONE", "UNDERLINE", "STRIKETHROUGH"
    ).several_of,
    "text_font": lv.font,
    "text_letter_space": cv.positive_int,
    "text_line_space": cv.positive_int,
    "text_opa": lv.opacity,
    "transform_angle": lv.angle,
    "transform_height": lv.pixels_or_percent,
    "transform_pivot_x": lv.pixels_or_percent,
    "transform_pivot_y": lv.pixels_or_percent,
    "transform_zoom": lv.zoom,
    "translate_x": lv.pixels_or_percent,
    "translate_y": lv.pixels_or_percent,
    "max_height": lv.pixels_or_percent,
    "max_width": lv.pixels_or_percent,
    "min_height": lv.pixels_or_percent,
    "min_width": lv.pixels_or_percent,
    "radius": cv.Any(lv.size, df.LvConstant("LV_RADIUS_", "CIRCLE").one_of),
    "width": lv.size,
    "x": lv.pixels_or_percent,
    "y": lv.pixels_or_percent,
}

STYLE_SCHEMA = cv.Schema({cv.Optional(k): v for k, v in STYLE_PROPS.items()}).extend(
    {
        cv.Optional(df.CONF_STYLES): cv.ensure_list(cv.use_id(ty.lv_style_t)),
        cv.Optional(df.CONF_SCROLLBAR_MODE): df.LvConstant(
            "LV_SCROLLBAR_MODE_", "OFF", "ON", "ACTIVE", "AUTO"
        ).one_of,
    }
)

STATE_SCHEMA = cv.Schema(
    {cv.Optional(state): STYLE_SCHEMA for state in df.STATES}
).extend(STYLE_SCHEMA)
SET_STATE_SCHEMA = cv.Schema({cv.Optional(state): lv.lv_bool for state in df.STATES})
FLAG_SCHEMA = cv.Schema({cv.Optional(flag): cv.boolean for flag in df.OBJ_FLAGS})
FLAG_LIST = cv.ensure_list(df.LvConstant("LV_OBJ_FLAG_", *df.OBJ_FLAGS).one_of)


def part_schema(parts):
    parts = WIDGET_PARTS.get(parts)
    if parts is None:
        parts = (df.CONF_MAIN,)
    return cv.Schema({cv.Optional(part): STATE_SCHEMA for part in parts}).extend(
        STATE_SCHEMA
    )


def automation_schema(typ: ty.LvType):
    if typ.has_on_value:
        events = df.LV_EVENT_TRIGGERS + (CONF_ON_VALUE,)
    else:
        events = df.LV_EVENT_TRIGGERS
    if isinstance(typ, ty.LvType):
        template = Trigger.template(typ.get_arg_type())
    else:
        template = Trigger.template()
    return {
        cv.Optional(event): validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(template),
            }
        )
        for event in events
    }


def create_modify_schema(widget_name, widget_type=lv_obj_t, extras: dict = None):
    schema = (
        part_schema(widget_name)
        .extend(
            {
                cv.Required(CONF_ID): cv.use_id(widget_type),
                cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
            }
        )
        .extend(FLAG_SCHEMA)
    )
    if extras is not None:
        return schema.extend(extras)
    return schema


def obj_schema(wtype: str):
    return (
        part_schema(wtype)
        .extend(FLAG_SCHEMA)
        .extend(LAYOUT_SCHEMA)
        .extend(automation_schema(ty.get_widget_type(wtype)))
        .extend(ALIGN_TO_SCHEMA)
        .extend(
            cv.Schema(
                {
                    cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
                    cv.Optional(CONF_GROUP): lv.id_name,
                }
            )
        )
    )


LAYOUT_SCHEMAS = {}


ALIGN_TO_SCHEMA = {
    cv.Optional(df.CONF_ALIGN_TO): cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_obj_t),
            cv.Required(df.CONF_ALIGN): df.ALIGN_ALIGNMENTS.one_of,
            cv.Optional(df.CONF_X, default=0): lv.pixels_or_percent,
            cv.Optional(df.CONF_Y, default=0): lv.pixels_or_percent,
        }
    )
}


def grid_free_space(value):
    value = cv.Upper(value)
    if value.startswith("FR(") and value.endswith(")"):
        value = value.removesuffix(")").removeprefix("FR(")
        return f"LV_GRID_FR({cv.positive_int(value)})"
    raise cv.Invalid("must be a size in pixels, CONTENT or FR(nn)")


grid_spec = cv.Any(
    lv.size, df.LvConstant("LV_GRID_", "CONTENT").one_of, grid_free_space
)

cell_alignments = df.LV_CELL_ALIGNMENTS.one_of
grid_alignments = df.LV_GRID_ALIGNMENTS.one_of
flex_alignments = df.LV_FLEX_ALIGNMENTS.one_of

LAYOUT_SCHEMA = {
    cv.Optional(df.CONF_LAYOUT): cv.typed_schema(
        {
            df.TYPE_GRID: {
                cv.Required(df.CONF_GRID_ROWS): [grid_spec],
                cv.Required(df.CONF_GRID_COLUMNS): [grid_spec],
                cv.Optional(df.CONF_GRID_COLUMN_ALIGN): grid_alignments,
                cv.Optional(df.CONF_GRID_ROW_ALIGN): grid_alignments,
            },
            df.TYPE_FLEX: {
                cv.Optional(
                    df.CONF_FLEX_FLOW, default="row_wrap"
                ): df.FLEX_FLOWS.one_of,
                cv.Optional(df.CONF_FLEX_ALIGN_MAIN, default="start"): flex_alignments,
                cv.Optional(df.CONF_FLEX_ALIGN_CROSS, default="start"): flex_alignments,
                cv.Optional(df.CONF_FLEX_ALIGN_TRACK, default="start"): flex_alignments,
            },
        },
        lower=True,
    )
}

GRID_CELL_SCHEMA = {
    cv.Required(df.CONF_GRID_CELL_ROW_POS): cv.positive_int,
    cv.Required(df.CONF_GRID_CELL_COLUMN_POS): cv.positive_int,
    cv.Optional(df.CONF_GRID_CELL_ROW_SPAN, default=1): cv.positive_int,
    cv.Optional(df.CONF_GRID_CELL_COLUMN_SPAN, default=1): cv.positive_int,
    cv.Optional(df.CONF_GRID_CELL_X_ALIGN): grid_alignments,
    cv.Optional(df.CONF_GRID_CELL_Y_ALIGN): grid_alignments,
}

FLEX_OBJ_SCHEMA = {
    cv.Optional(df.CONF_FLEX_GROW): cv.int_,
}


STYLED_TEXT_SCHEMA = cv.maybe_simple_value(
    STYLE_SCHEMA.extend(TEXT_SCHEMA), key=df.CONF_TEXT
)


# For use by platform components
LVGL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(df.CONF_LVGL_ID): cv.use_id(ty.LvglComponent),
    }
)

ALL_STYLES = {**STYLE_PROPS, **GRID_CELL_SCHEMA, **FLEX_OBJ_SCHEMA}


def container_validator(schema, widget_type):
    def validator(value):
        result = schema
        if w_sch := WIDGET_TYPES[widget_type].schema:
            result = result.extend(w_sch)
        ltype = df.TYPE_NONE
        if value and (layout := value.get(df.CONF_LAYOUT)):
            if not isinstance(layout, dict):
                raise cv.Invalid("Layout value must be a dict")
            ltype = layout.get(CONF_TYPE)
            add_lv_use(ltype)
        result = result.extend(LAYOUT_SCHEMAS[ltype.lower()])
        if value == SCHEMA_EXTRACT:
            return result
        return result(value)

    return validator


def container_schema(widget_type, extras=None):
    """
    Create a schema for a container widget of a given type. All obj properties are available, plus
    the extras passed in, plus any defined for the specific widget being specified.
    :param widget_type:     The widget type, e.g. "img"
    :param extras:  Additional options to be made available, e.g. layout properties for children
    :return: The schema for this type of widget.
    """
    lv_type = get_widget_type(widget_type)
    schema = obj_schema(widget_type).extend({cv.GenerateID(): cv.declare_id(lv_type)})
    if extras:
        schema = schema.extend(extras)
    # Delayed evaluation for recursion
    return container_validator(schema, widget_type)


def widget_schema(name, extras=None):
    validator = container_schema(name, extras=extras)
    if required := REQUIRED_COMPONENTS.get(name):
        validator = cv.All(validator, lv.requires_component(required))
    return cv.Exclusive(name, df.CONF_WIDGETS), validator


# All widget schemas must be defined before this.


def any_widget_schema(extras=None):
    """
    Generate schemas for all possible LVGL widgets. This is what implements the ability to have a list of any kind of
    widget under the widgets: key.

    :param extras: Additional schema to be applied to each generated one
    :return:
    """
    return cv.Any(dict(widget_schema(wt, extras) for wt in WIDGET_PARTS))


ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.Required(CONF_ID): cv.use_id(ty.lv_pseudo_button_t),
    },
    key=CONF_ID,
)

ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ty.LVEncoderListener),
        cv.Optional(CONF_GROUP): lv.id_name,
        cv.Optional(df.CONF_LONG_PRESS_TIME, default="400ms"): lv.lv_milliseconds,
        cv.Optional(
            df.CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
        ): lv.lv_milliseconds,
    }
)
