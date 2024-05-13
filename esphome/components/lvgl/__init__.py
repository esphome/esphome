import functools
import re
import logging
from typing import Any

from esphome.core import (
    CORE,
    ID,
    Lambda,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import (
    automation,
)
from esphome.components.image import Image_
from esphome.coroutine import FakeAwaitable
from esphome.components.touchscreen import (
    Touchscreen,
    CONF_TOUCHSCREEN_ID,
)
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.components.display import Display
from esphome.components.rotary_encoder.sensor import RotaryEncoderSensor
from esphome.components.binary_sensor import BinarySensor
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_BRIGHTNESS,
    CONF_BUFFER_SIZE,
    CONF_COLOR,
    CONF_COUNT,
    CONF_GROUP,
    CONF_ID,
    CONF_LED,
    CONF_LENGTH,
    CONF_LOCAL,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_MODE,
    CONF_OPTIONS,
    CONF_PAGES,
    CONF_RANGE_FROM,
    CONF_RANGE_TO,
    CONF_ROTATION,
    CONF_SENSOR,
    CONF_STATE,
    CONF_TIME,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
    CONF_VALUE,
    CONF_WIDTH,
    CONF_ON_VALUE,
    CONF_ARGS,
    CONF_FORMAT,
    CONF_DURATION,
    CONF_STEP,
    CONF_ANGLE,
    CONF_ROW,
    CONF_DISPLAY_ID,
    CONF_ON_IDLE,
    CONF_MAX_LENGTH,
    CONF_TYPE,
    CONF_NAME,
    CONF_POSITION,
)
from esphome.cpp_generator import LambdaExpression

from . import types as ty

from . import defines as df

from . import lv_validation as lv

from .widget import (
    Widget,
    add_temp_var,
    lv_temp_vars,
    MatrixButton,
)
from ..sensor import Sensor
from ..text_sensor import TextSensor
from ...helpers import cpp_string_escape

DOMAIN = "lvgl"
DEPENDENCIES = ("display",)
AUTO_LOAD = ("key_provider",)
CODEOWNERS = ("@clydebarrow",)
LOGGER = logging.getLogger(__name__)

# list of widgets and the parts allowed
WIDGET_TYPES = {
    df.CONF_ANIMIMG: (df.CONF_MAIN,),
    df.CONF_ARC: (df.CONF_MAIN, df.CONF_INDICATOR, df.CONF_KNOB),
    df.CONF_BTN: (df.CONF_MAIN,),
    df.CONF_BAR: (df.CONF_MAIN, df.CONF_INDICATOR),
    df.CONF_BTNMATRIX: (df.CONF_MAIN, df.CONF_ITEMS),
    df.CONF_CANVAS: (df.CONF_MAIN,),
    df.CONF_CHART: (
        df.CONF_MAIN,
        df.CONF_SCROLLBAR,
        df.CONF_SELECTED,
        df.CONF_ITEMS,
        df.CONF_INDICATOR,
        df.CONF_CURSOR,
        df.CONF_TICKS,
    ),
    df.CONF_CHECKBOX: (df.CONF_MAIN, df.CONF_INDICATOR),
    df.CONF_DROPDOWN: (df.CONF_MAIN, df.CONF_INDICATOR),
    df.CONF_IMG: (df.CONF_MAIN,),
    df.CONF_INDICATOR: (),
    df.CONF_KEYBOARD: (df.CONF_MAIN, df.CONF_ITEMS),
    df.CONF_LABEL: (df.CONF_MAIN, df.CONF_SCROLLBAR, df.CONF_SELECTED),
    CONF_LED: (df.CONF_MAIN,),
    df.CONF_LINE: (df.CONF_MAIN,),
    df.CONF_DROPDOWN_LIST: (df.CONF_MAIN, df.CONF_SCROLLBAR, df.CONF_SELECTED),
    df.CONF_METER: (df.CONF_MAIN,),
    df.CONF_OBJ: (df.CONF_MAIN,),
    # df.CONF_PAGE: (df.CONF_MAIN,),
    df.CONF_ROLLER: (df.CONF_MAIN, df.CONF_SELECTED),
    df.CONF_SLIDER: (df.CONF_MAIN, df.CONF_INDICATOR, df.CONF_KNOB),
    df.CONF_SPINNER: (df.CONF_MAIN, df.CONF_INDICATOR),
    df.CONF_SWITCH: (df.CONF_MAIN, df.CONF_INDICATOR, df.CONF_KNOB),
    df.CONF_SPINBOX: (
        df.CONF_MAIN,
        df.CONF_SCROLLBAR,
        df.CONF_SELECTED,
        df.CONF_CURSOR,
        df.CONF_TEXTAREA_PLACEHOLDER,
    ),
    df.CONF_TABLE: (df.CONF_MAIN, df.CONF_ITEMS),
    df.CONF_TABVIEW: (df.CONF_MAIN,),
    df.CONF_TEXTAREA: (
        df.CONF_MAIN,
        df.CONF_SCROLLBAR,
        df.CONF_SELECTED,
        df.CONF_CURSOR,
        df.CONF_TEXTAREA_PLACEHOLDER,
    ),
    df.CONF_TILEVIEW: (df.CONF_MAIN),
}


class LValidator:
    def __init__(self, validator, rtype, idtype=None, idexpr=None, retmapper=None):
        self.validator = validator
        self.rtype = rtype
        self.idtype = idtype
        self.idexpr = idexpr
        self.retmapper = retmapper

    def __call__(self, value):
        if isinstance(value, cv.Lambda):
            return cv.returning_lambda(value)
        if self.idtype is not None and isinstance(value, ID):
            return cv.use_id(self.idtype)(value)
        return self.validator(value)

    async def process(self, value, args=()):
        if value is None:
            return None
        if isinstance(value, Lambda):
            return f"{await cg.process_lambda(value, args, return_type=self.rtype)}()"
        if self.idtype is not None and isinstance(value, ID):
            return f"{value}->{self.idexpr};"
        if self.retmapper is not None:
            return self.retmapper(value)
        return value


lv_color = LValidator(lv.color, ty.lv_color_t, retmapper=lv.color_retmapper)
lv_bool = LValidator(lv.bool_, cg.bool_, BinarySensor, "get_state()")
lv_milliseconds = LValidator(
    cv.positive_time_period_milliseconds,
    cg.int32,
    retmapper=lambda x: x.total_milliseconds,
)


class TextValidator(LValidator):
    def __init__(self):
        super().__init__(
            cv.string,
            cg.const_char_ptr,
            TextSensor,
            "get_state().c_str()",
            lambda s: cg.safe_exp(f"{s}"),
        )

    def __call__(self, value):
        if isinstance(value, dict):
            return value
        return super().__call__(value)

    async def set_text(self, var: Widget, value, propname: str = df.CONF_TEXT):
        """
        Set the text property
        :param var:     The widget
        :param value:   The value to be set.
        :return: The generated code
        """

        if isinstance(value, dict):
            args = [str(x) for x in value[CONF_ARGS]]
            arg_expr = cg.RawExpression(",".join(args))
            format = cpp_string_escape(value[CONF_FORMAT])
            return var.set_property(
                "text", f"str_sprintf({format}, {arg_expr}).c_str()"
            )
        return var.set_property(propname, await self.process(value))


lv_text = TextValidator()
lv_float = LValidator(cv.float_, cg.float_, Sensor, "get_state()")
lv_int = LValidator(cv.int_, cg.int_, Sensor, "get_state()")
lv_brightness = LValidator(
    cv.percentage, cg.float_, Sensor, "get_state()", retmapper=lambda x: int(x * 255)
)

cell_alignments = df.LV_CELL_ALIGNMENTS.one_of
grid_alignments = df.LV_GRID_ALIGNMENTS.one_of
flex_alignments = df.LV_FLEX_ALIGNMENTS.one_of

STYLE_PROPS = {
    "align": df.CHILD_ALIGNMENTS.one_of,
    "arc_opa": lv.opacity,
    "arc_color": lv_color,
    "arc_rounded": lv_bool,
    "arc_width": cv.positive_int,
    "anim_time": lv_milliseconds,
    "bg_color": lv_color,
    "bg_grad_color": lv_color,
    "bg_dither_mode": df.LvConstant("LV_DITHER_", "NONE", "ORDERED", "ERR_DIFF").one_of,
    "bg_grad_dir": df.LvConstant("LV_GRAD_DIR_", "NONE", "HOR", "VER").one_of,
    "bg_grad_stop": lv.stop_value,
    "bg_img_opa": lv.opacity,
    "bg_img_recolor": lv_color,
    "bg_img_recolor_opa": lv.opacity,
    "bg_main_stop": lv.stop_value,
    "bg_opa": lv.opacity,
    "border_color": lv_color,
    "border_opa": lv.opacity,
    "border_post": cv.boolean,
    "border_side": df.LvConstant(
        "LV_BORDER_SIDE_", "NONE", "TOP", "BOTTOM", "LEFT", "RIGHT", "INTERNAL"
    ).several_of,
    "border_width": cv.positive_int,
    "clip_corner": lv_bool,
    "height": lv.size,
    "img_recolor": lv_color,
    "img_recolor_opa": lv.opacity,
    "line_width": cv.positive_int,
    "line_dash_width": cv.positive_int,
    "line_dash_gap": cv.positive_int,
    "line_rounded": lv_bool,
    "line_color": lv_color,
    "opa": lv.opacity,
    "opa_layered": lv.opacity,
    "outline_color": lv_color,
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
    "shadow_color": lv_color,
    "shadow_ofs_x": cv.int_,
    "shadow_ofs_y": cv.int_,
    "shadow_opa": lv.opacity,
    "shadow_spread": cv.int_,
    "shadow_width": cv.positive_int,
    "text_align": df.LvConstant(
        "LV_TEXT_ALIGN_", "LEFT", "CENTER", "RIGHT", "AUTO"
    ).one_of,
    "text_color": lv_color,
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


def validate_max_min(config):
    if CONF_MAX_VALUE in config and CONF_MIN_VALUE in config:
        if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
            raise cv.Invalid("max_value must be greater than min_value")
    return config


def modify_schema(widget_type):
    lv_type = ty.get_widget_type(widget_type)
    schema = (
        part_schema(widget_type)
        .extend(
            {
                cv.Required(CONF_ID): cv.use_id(lv_type),
                cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
            }
        )
        .extend(FLAG_SCHEMA)
    )
    if extras := globals().get(f"{widget_type.upper()}_MODIFY_SCHEMA"):
        return schema.extend(extras)
    if extras := globals().get(f"{widget_type.upper()}_SCHEMA"):
        return schema.extend(extras)
    return schema


def generate_id(base):
    generate_id.counter += 1
    return f"lvgl_{base}_{generate_id.counter}"


generate_id.counter = 0


def cv_point_list(value):
    if not isinstance(value, list):
        raise cv.Invalid("List of points required")
    values = list(map(lv.point_list, value))
    if not functools.reduce(lambda f, v: f and len(v) == 2, values, True):
        raise cv.Invalid("Points must be a list of x,y integer pairs")
    lv.lv_uses.add("POINT")
    return {
        CONF_ID: cv.declare_id(ty.lv_point_t)(generate_id(df.CONF_POINTS)),
        df.CONF_POINTS: values,
    }


def part_schema(parts):
    if isinstance(parts, str) and parts in WIDGET_TYPES:
        parts = WIDGET_TYPES[parts]
    else:
        parts = (df.CONF_MAIN,)
    return cv.Schema({cv.Optional(part): STATE_SCHEMA for part in parts}).extend(
        STATE_SCHEMA
    )


def automation_schema(type: ty.LvType):
    if type.has_on_value:
        events = df.LV_EVENT_TRIGGERS + (CONF_ON_VALUE,)
    else:
        events = df.LV_EVENT_TRIGGERS
    if isinstance(type, ty.LvType):
        template = automation.Trigger.template(type.get_arg_type())
    else:
        template = automation.Trigger.template()
    return {
        cv.Optional(event): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(template),
            }
        )
        for event in events
    }


# https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
def validate_printf(value):
    cfmt = r"""
    (                                  # start of capture group 1
    %                                  # literal "%"
    (?:[-+0 #]{0,5})                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    [cCdiouxXeEfgGaAnpsSZ]             # type
    )
    """  # noqa
    matches = re.findall(cfmt, value[CONF_FORMAT], flags=re.X)
    if len(matches) != len(value[CONF_ARGS]):
        raise cv.Invalid(
            f"Found {len(matches)} printf-patterns ({', '.join(matches)}), but {len(value[CONF_ARGS])} args were given!"
        )
    return value


# Map typenames to action types and templates


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
            lv_text,
        )
    }
)

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
SET_STATE_SCHEMA = cv.Schema({cv.Optional(state): lv_bool for state in df.STATES})
FLAG_SCHEMA = cv.Schema({cv.Optional(flag): cv.boolean for flag in df.OBJ_FLAGS})
FLAG_LIST = cv.ensure_list(df.LvConstant("LV_OBJ_FLAG_", df.OBJ_FLAGS).one_of)

BAR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.int_,
        cv.Optional(CONF_MAX_VALUE, default=100): cv.int_,
        cv.Optional(CONF_MODE, default="NORMAL"): df.BAR_MODES.one_of,
        cv.Optional(df.CONF_ANIMATED, default=True): lv.animated,
    }
)

SLIDER_SCHEMA = BAR_SCHEMA

LINE_SCHEMA = {cv.Optional(df.CONF_POINTS): cv_point_list}


def lv_repeat_count(value):
    if isinstance(value, str) and value.lower() in ("forever", "infinite"):
        value = 0xFFFF
    return cv.positive_int(value)


def validate_spinbox(config):
    max_val = 2**31 - 1
    min_val = -1 - max_val
    scale = 10 ** config[df.CONF_DECIMAL_PLACES]
    range_from = int(config[CONF_RANGE_FROM] * scale)
    range_to = int(config[CONF_RANGE_TO] * scale)
    step = int(config[CONF_STEP] * scale)
    if (
        range_from > max_val
        or range_from < min_val
        or range_to > max_val
        or range_to < min_val
    ):
        raise cv.Invalid("Range outside allowed limits")
    if step <= 0 or step >= (range_to - range_from) / 2:
        raise cv.Invalid("Invalid step value")
    if config[df.CONF_DIGITS] <= config[df.CONF_DECIMAL_PLACES]:
        raise cv.Invalid("Number of digits must exceed number of decimal places")
    return config


ANIMIMG_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_REPEAT_COUNT, default="forever"): lv_repeat_count,
        cv.Optional(df.CONF_AUTO_START, default=True): cv.boolean,
    }
)
ANIMIMG_SCHEMA = ANIMIMG_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_DURATION): lv_milliseconds,
        cv.Required(df.CONF_SRC): cv.ensure_list(cv.use_id(Image_)),
    }
)

ANIMIMG_MODIFY_SCHEMA = ANIMIMG_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_DURATION): lv_milliseconds,
    }
)

IMG_SCHEMA = {
    cv.Required(df.CONF_SRC): cv.use_id(Image_),
    cv.Optional(df.CONF_PIVOT_X, default="50%"): lv.size,
    cv.Optional(df.CONF_PIVOT_Y, default="50%"): lv.size,
    cv.Optional(CONF_ANGLE): lv.angle,
    cv.Optional(df.CONF_ZOOM): lv.zoom,
    cv.Optional(df.CONF_OFFSET_X): lv.size,
    cv.Optional(df.CONF_OFFSET_Y): lv.size,
    cv.Optional(df.CONF_ANTIALIAS): lv_bool,
    cv.Optional(CONF_MODE): df.LvConstant(
        "LV_IMG_SIZE_MODE_", "VIRTUAL", "REAL"
    ).one_of,
}

# Schema for a single button in a btnmatrix
BTNM_BTN_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_TEXT): cv.string,
        cv.Optional(df.CONF_KEY_CODE): lv.key_code,
        cv.GenerateID(): cv.declare_id(ty.LvBtnmBtn),
        cv.Optional(CONF_WIDTH, default=1): cv.positive_int,
        cv.Optional(df.CONF_CONTROL): cv.ensure_list(
            cv.Schema({cv.Optional(k.lower()): cv.boolean for k in df.BTNMATRIX_CTRLS})
        ),
    }
).extend(automation_schema(ty.lv_btn_t))

BTNMATRIX_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_ONE_CHECKED, default=False): lv_bool,
        cv.Required(df.CONF_ROWS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(df.CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
                }
            )
        ),
    }
)

BTNMATRIX_MODIFY_SCHEMA = cv.Schema({})

ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.int_,
        cv.Optional(CONF_MAX_VALUE, default=100): cv.int_,
        cv.Optional(df.CONF_START_ANGLE, default=135): lv.angle,
        cv.Optional(df.CONF_END_ANGLE, default=45): lv.angle,
        cv.Optional(CONF_ROTATION, default=0.0): lv.angle,
        cv.Optional(df.CONF_ADJUSTABLE, default=False): bool,
        cv.Optional(CONF_MODE, default="NORMAL"): df.ARC_MODES.one_of,
        cv.Optional(df.CONF_CHANGE_RATE, default=720): cv.uint16_t,
    }
)

ARC_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
    }
)

BAR_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(df.CONF_ANIMATED, default=True): lv.animated,
    }
)

SLIDER_MODIFY_SCHEMA = BAR_MODIFY_SCHEMA

SPINNER_SCHEMA = cv.Schema(
    {
        cv.Required(df.CONF_ARC_LENGTH): lv.angle,
        cv.Required(df.CONF_SPIN_TIME): cv.positive_time_period_milliseconds,
    }
)

SPINNER_MODIFY_SCHEMA = cv.Schema({})

INDICATOR_LINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv.size,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(df.CONF_R_MOD, default=0): lv.size,
        cv.Optional(CONF_VALUE): lv_float,
    }
)
INDICATOR_IMG_SCHEMA = cv.Schema(
    {
        cv.Required(df.CONF_SRC): cv.use_id(Image_),
        cv.Required(df.CONF_PIVOT_X): lv.pixels,
        cv.Required(df.CONF_PIVOT_Y): lv.pixels,
        cv.Optional(CONF_VALUE): lv_float,
    }
)
INDICATOR_ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv.size,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(df.CONF_R_MOD, default=0): lv.size,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
        cv.Exclusive(df.CONF_START_VALUE, CONF_VALUE): lv_float,
        cv.Optional(df.CONF_END_VALUE): lv_float,
    }
)
INDICATOR_TICKS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv.size,
        cv.Optional(df.CONF_COLOR_START, default=0): lv_color,
        cv.Optional(df.CONF_COLOR_END): lv_color,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
        cv.Exclusive(df.CONF_START_VALUE, CONF_VALUE): lv_float,
        cv.Optional(df.CONF_END_VALUE): lv_float,
        cv.Optional(CONF_LOCAL, default=False): lv_bool,
    }
)
INDICATOR_SCHEMA = cv.Schema(
    {
        cv.Exclusive(df.CONF_LINE, df.CONF_INDICATORS): INDICATOR_LINE_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(ty.lv_meter_indicator_t),
            }
        ),
        cv.Exclusive(df.CONF_IMG, df.CONF_INDICATORS): INDICATOR_IMG_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(ty.lv_meter_indicator_t),
            }
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
                cv.Optional(CONF_COLOR, default=0x808080): lv_color,
                cv.Optional(df.CONF_MAJOR): cv.Schema(
                    {
                        cv.Optional(df.CONF_STRIDE, default=3): cv.positive_int,
                        cv.Optional(CONF_WIDTH, default=5): lv.size,
                        cv.Optional(CONF_LENGTH, default="15%"): lv.size,
                        cv.Optional(CONF_COLOR, default=0): lv_color,
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

STYLED_TEXT_SCHEMA = cv.maybe_simple_value(
    STYLE_SCHEMA.extend(TEXT_SCHEMA), key=df.CONF_TEXT
)
PAGE_SCHEMA = {
    cv.Optional(df.CONF_SKIP, default=False): lv_bool,
}

LABEL_SCHEMA = TEXT_SCHEMA.extend(
    {
        cv.Optional(df.CONF_RECOLOR): lv_bool,
        cv.Optional(df.CONF_LONG_MODE): df.LV_LONG_MODES.one_of,
    }
)

TEXTAREA_SCHEMA = TEXT_SCHEMA.extend(
    {
        cv.Optional(df.CONF_PLACEHOLDER_TEXT): lv_text,
        cv.Optional(df.CONF_ACCEPTED_CHARS): lv_text,
        cv.Optional(df.CONF_ONE_LINE): lv_bool,
        cv.Optional(df.CONF_PASSWORD_MODE): lv_bool,
        cv.Optional(CONF_MAX_LENGTH): lv_int,
    }
)

CHECKBOX_SCHEMA = TEXT_SCHEMA

DROPDOWN_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_SYMBOL): lv_text,
        cv.Optional(df.CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(df.CONF_DIR, default="BOTTOM"): df.DIRECTIONS.one_of,
        cv.Optional(df.CONF_DROPDOWN_LIST): part_schema(df.CONF_DROPDOWN_LIST),
    }
)

DROPDOWN_SCHEMA = DROPDOWN_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(lv.option_string),
    }
)

DROPDOWN_MODIFY_SCHEMA = DROPDOWN_BASE_SCHEMA

ROLLER_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(df.CONF_VISIBLE_ROW_COUNT): lv_int,
        cv.Optional(CONF_MODE, default="NORMAL"): df.ROLLER_MODES.one_of,
    }
)

ROLLER_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(lv.option_string),
    }
)

ROLLER_MODIFY_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Optional(df.CONF_ANIMATED, default=True): lv.animated,
    }
)

LED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_BRIGHTNESS): lv_brightness,
    }
)

SPIN_ACTIONS = (
    "INCREMENT",
    "DECREMENT",
    "STEP_NEXT",
    "STEP_PREV",
    "CLEAR",
)

SPINBOX_SCHEMA = {
    cv.Optional(CONF_VALUE): lv_float,
    cv.Optional(CONF_RANGE_FROM, default=0): cv.float_,
    cv.Optional(CONF_RANGE_TO, default=100): cv.float_,
    cv.Optional(df.CONF_DIGITS, default=4): cv.int_range(1, 10),
    cv.Optional(CONF_STEP, default=1.0): cv.positive_float,
    cv.Optional(df.CONF_DECIMAL_PLACES, default=0): cv.int_range(0, 6),
    cv.Optional(df.CONF_ROLLOVER, default=False): lv_bool,
}

SPINBOX_MODIFY_SCHEMA = {
    cv.Required(CONF_VALUE): lv_float,
}

KEYBOARD_SCHEMA = {
    cv.Optional(CONF_MODE, default="TEXT_UPPER"): df.KEYBOARD_MODES.one_of,
    cv.Optional(df.CONF_TEXTAREA): cv.use_id(ty.lv_textarea_t),
}

# For use by platform components
LVGL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(df.CONF_LVGL_ID): cv.use_id(ty.LvglComponent),
    }
)

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

LAYOUT_SCHEMA = {
    cv.Optional(df.CONF_LAYOUT): cv.typed_schema(
        {
            df.TYPE_GRID: {
                cv.Required(df.CONF_GRID_ROWS): grid_spec,
                cv.Required(df.CONF_GRID_COLUMNS): grid_spec,
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
        }
    )
}

GRID_CELL_SCHEMA = {
    cv.Optional(df.CONF_GRID_CELL_X_ALIGN, default="start"): cell_alignments,
    cv.Optional(df.CONF_GRID_CELL_Y_ALIGN, default="start"): cell_alignments,
    cv.Optional(df.CONF_GRID_CELL_ROW_POS, default=1): cv.positive_int,
    cv.Optional(df.CONF_GRID_CELL_COLUMN_POS, default=1): cv.positive_int,
    cv.Optional(df.CONF_GRID_CELL_ROW_SPAN, default=1): cv.positive_int,
    cv.Optional(df.CONF_GRID_CELL_COLUMN_SPAN, default=1): cv.positive_int,
}

FLEX_OBJ_SCHEMA = {
    cv.Optional(df.CONF_FLEX_GROW): cv.int_,
}


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


WIDGET_SCHEMAS = {}


def container_schema(widget_type, extras=None):
    lv_type = ty.get_widget_type(widget_type)
    schema = obj_schema(widget_type).extend({cv.GenerateID(): cv.declare_id(lv_type)})
    if globs := globals().get(f"{widget_type.upper()}_SCHEMA"):
        schema = schema.extend(globs)
    if extras:
        schema = schema.extend(extras)

    # Delayed evaluation for recursion
    def validator(value):
        result = schema
        ltype = df.TYPE_NONE
        if layout := value.get(df.CONF_LAYOUT):
            if not isinstance(layout, dict):
                raise cv.Invalid("Layout value must be a dict")
            ltype = layout.get(CONF_TYPE)
        result = result.extend(WIDGET_SCHEMAS[ltype])
        if value == SCHEMA_EXTRACT:
            return result
        return result(value)

    return validator


def widget_schema(name, extras=None):
    validator = container_schema(name, extras=extras)
    if required := lv.REQUIRED_COMPONENTS.get(name):
        validator = cv.All(validator, lv.requires_component(required))
    return cv.Exclusive(name, df.CONF_WIDGETS), validator


def any_widget_schema(extras=None):
    return cv.Any(dict(map(lambda wt: widget_schema(wt, extras), WIDGET_TYPES)))


TILE_SCHEMA = any_widget_schema(
    {
        cv.Required(CONF_ROW): lv_int,
        cv.Required(df.CONF_COLUMN): lv_int,
        cv.GenerateID(df.CONF_TILE_ID): cv.declare_id(ty.lv_tile_t),
        cv.Optional(df.CONF_DIR, default="ALL"): df.TILE_DIRECTIONS.several_of,
    }
)

TILEVIEW_SCHEMA = {
    cv.Required(df.CONF_TILES): cv.ensure_list(TILE_SCHEMA),
    cv.Optional(CONF_ON_VALUE): automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                automation.Trigger.template(ty.lv_obj_t_ptr)
            )
        }
    ),
}

TAB_SCHEMA = any_widget_schema(
    {
        cv.Required(CONF_NAME): cv.string,
        cv.GenerateID(df.CONF_TILE_ID): cv.declare_id(ty.lv_tab_t),
        cv.Optional(CONF_POSITION, default="ALL"): df.DIRECTIONS.one_of,
    }
)
TABVIEW_SCHEMA = {
    cv.Required(df.CONF_TABS): cv.ensure_list(TAB_SCHEMA),
    cv.Optional(CONF_ON_VALUE): automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                automation.Trigger.template(ty.lv_obj_t_ptr)
            )
        }
    ),
}

WIDGET_SCHEMA = any_widget_schema()

MSGBOX_SCHEMA = STYLE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ty.lv_obj_t),
        cv.Required(df.CONF_TITLE): STYLED_TEXT_SCHEMA,
        cv.Optional(df.CONF_BODY): STYLED_TEXT_SCHEMA,
        cv.Optional(df.CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
        cv.Optional(df.CONF_CLOSE_BUTTON): lv_bool,
        cv.Optional(df.CONF_WIDGETS): cv.ensure_list(WIDGET_SCHEMA),
    }
)

# All widget schemas must be defined before this.

WIDGET_SCHEMAS[df.TYPE_GRID] = {
    cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema(GRID_CELL_SCHEMA))
}
WIDGET_SCHEMAS[df.TYPE_FLEX] = {
    cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema(FLEX_OBJ_SCHEMA))
}
WIDGET_SCHEMAS[df.TYPE_NONE] = {
    cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema())
}

ALL_STYLES = {**STYLE_PROPS, **GRID_CELL_SCHEMA, **FLEX_OBJ_SCHEMA}


async def get_color_value(value):
    if isinstance(value, Lambda):
        return f"{await cg.process_lambda(value, [], return_type=ty.lv_color_t)}()"
    return value


async def get_end_value(config):
    return await lv_int.process(config.get(df.CONF_END_VALUE))


async def get_start_value(config):
    if df.CONF_START_VALUE in config:
        value = config[df.CONF_START_VALUE]
    else:
        value = config.get(CONF_VALUE)
    return await lv_int.process(value)


async def add_init_lambda(lv_component, init):
    lamb = await cg.process_lambda(
        Lambda(";\n".join([*init, ""])), [(ty.lv_disp_t_ptr, "lv_disp")]
    )
    cg.add(lv_component.add_init_lambda(lamb))
    lv_temp_vars.clear()


def cgen(*args):
    cg.add(cg.RawExpression("\n".join(args)))


async def styles_to_code(styles):
    """Convert styles to C__ code."""
    for style in styles:
        svar = cg.new_Pvariable(style[CONF_ID])
        cgen(f"lv_style_init({svar})")
        for prop, validator in STYLE_PROPS.items():
            if value := style.get(prop):
                if isinstance(validator, LValidator):
                    value = await validator.process(value)

                if isinstance(value, list):
                    value = "|".join(value)
                cgen(f"lv_style_set_{prop}({svar}, {value})")


theme_widget_map = {}
# Map of widgets to their config, used for trigger generation
widget_map: dict[Any, Widget] = {}
widgets_completed = False  # will be set true when all widgets are available


def get_widget_generator(wid):
    while True:
        if obj := widget_map.get(wid):
            return obj
        if widgets_completed:
            raise cv.Invalid(
                f"Widget {wid} not found, yet all widgets should be defined by now"
            )
        yield


async def get_widget(wid: ID) -> Widget:
    if obj := widget_map.get(wid):
        return obj
    return await FakeAwaitable(get_widget_generator(wid))


async def theme_to_code(theme):
    for widg, style in theme.items():
        if not isinstance(style, dict):
            continue

        init = []
        ow = Widget("obj", ty.get_widget_type(widg))
        init.extend(await set_obj_properties(ow, style))
        lamb = await cg.process_lambda(
            Lambda(";\n".join([*init, ""])),
            [(ty.lv_obj_t_ptr, "obj")],
            capture="",
        )
        apply = f"lv_theme_apply_{widg}"
        theme_widget_map[widg] = apply
        lamb_id = ID(apply, type=ty.lv_lambda_t, is_declaration=True)
        cg.variable(lamb_id, lamb)


lv_groups = set()  # Widget group names
lv_defines = {}  # Dict of #defines to provide as build flags


def add_group(name):
    if name is None:
        return None
    fullname = f"lv_esp_group_{name}"
    if name not in lv_groups:
        cgen(f"static lv_group_t * {fullname} = lv_group_create()")
        lv_groups.add(name)
    return fullname


def add_define(macro, value="1"):
    if macro in lv_defines and lv_defines[macro] != value:
        LOGGER.error(
            "Redefinition of %s - was %s now %s", macro, lv_defines[macro], value
        )
    lv_defines[macro] = value


def collect_props(config):
    props = {}
    for prop in [*ALL_STYLES, *df.OBJ_FLAGS, df.CONF_STYLES, CONF_GROUP]:
        if prop in config:
            props[prop] = config[prop]
    return props


def collect_states(config):
    states = {df.CONF_DEFAULT: collect_props(config)}
    for state in df.STATES:
        if state in config:
            states[state] = collect_props(config[state])
    return states


def collect_parts(config):
    parts = {df.CONF_MAIN: collect_states(config)}
    for part in df.PARTS:
        if part in config:
            parts[part] = collect_states(config[part])
    return parts


async def set_obj_properties(widg: Widget, config):
    """Return a list of C++ statements to apply properties to an ty.lv_obj_t"""
    init = []
    if layout := config.get(df.CONF_LAYOUT):
        layout_type: str = layout[CONF_TYPE].upper()
        lv.lv_uses.add(layout_type)
        init.extend(
            widg.set_property(df.CONF_LAYOUT, f"LV_LAYOUT_{layout_type}", ltype="obj")
        )
        if layout_type.lower() == df.TYPE_GRID:
            wid = config[CONF_ID]
            rows = (
                "{" + ",".join(layout[df.CONF_GRID_ROWS]) + ", LV_GRID_TEMPLATE_LAST}"
            )
            row_id = ID(f"{wid}_row_dsc", is_declaration=True, type=ty.lv_coord_t)
            row_array = cg.static_const_array(row_id, cg.RawExpression(rows))
            init.extend(widg.set_style("grid_row_dsc_array", row_array, 0))
            columns = (
                "{"
                + ",".join(layout[df.CONF_GRID_COLUMNS])
                + ", LV_GRID_TEMPLATE_LAST}"
            )
            column_id = ID(f"{wid}_column_dsc", is_declaration=True, type=ty.lv_coord_t)
            column_array = cg.static_const_array(column_id, cg.RawExpression(columns))
            init.extend(widg.set_style("grid_column_dsc_array", column_array, 0))
        if layout_type.lower() == df.TYPE_FLEX:
            lv.lv_uses.add(df.TYPE_FLEX)
            init.extend(widg.set_property(df.CONF_FLEX_FLOW, layout, ltype="obj"))
            main = layout[df.CONF_FLEX_ALIGN_MAIN]
            cross = layout[df.CONF_FLEX_ALIGN_CROSS]
            track = layout[df.CONF_FLEX_ALIGN_TRACK]
            init.append(f"lv_obj_set_flex_align({widg.obj}, {main}, {cross}, {track})")
    parts = collect_parts(config)
    for part, states in parts.items():
        for state, props in states.items():
            lv_state = f"(int)LV_STATE_{state.upper()}|(int)LV_PART_{part.upper()}"
            if styles := props.get(df.CONF_STYLES):
                for style_id in styles:
                    init.append(f"lv_obj_add_style({widg.obj}, {style_id}, {lv_state})")
            for prop, value in {
                k: v for k, v in props.items() if k in ALL_STYLES
            }.items():
                if isinstance(ALL_STYLES[prop], LValidator):
                    value = await ALL_STYLES[prop].process(value)
                init.extend(widg.set_style(prop, value, lv_state))
    if group := add_group(config.get(CONF_GROUP)):
        init.append(f"lv_group_add_obj({group}, {widg.obj})")
    flag_clr = set()
    flag_set = set()
    props = parts[df.CONF_MAIN][df.CONF_DEFAULT]
    for prop, value in {k: v for k, v in props.items() if k in df.OBJ_FLAGS}.items():
        if value:
            flag_set.add(prop)
        else:
            flag_clr.add(prop)
    if flag_set:
        adds = lv.join_enums(flag_set, "LV_OBJ_FLAG_")
        init.extend(widg.add_flag(adds))
    if flag_clr:
        clrs = lv.join_enums(flag_clr, "LV_OBJ_FLAG_")
        init.extend(widg.clear_flag(clrs))

    if states := config.get(CONF_STATE):
        adds = set()
        clears = set()
        lambs = {}
        for key, value in states.items():
            if isinstance(value, cv.Lambda):
                lambs[key] = value
            elif value == "true":
                adds.add(key)
            else:
                clears.add(key)
        if adds:
            adds = lv.join_enums(adds, "LV_STATE_")
            init.extend(widg.add_state(adds))
        if clears:
            clears = lv.join_enums(clears, "LV_STATE_")
            init.extend(widg.clear_state(clears))
        for key, value in lambs.items():
            lamb = await cg.process_lambda(value, [], return_type=cg.bool_)
            init.append(
                f"""
                if({lamb}())
                    lv_obj_add_state({widg.obj}, LV_STATE_{key.upper()});
                else
                    lv_obj_clear_state({widg.obj}, LV_STATE_{key.upper()});
                """
            )
    if scrollbar_mode := config.get(df.CONF_SCROLLBAR_MODE):
        init.append(f"lv_obj_set_scrollbar_mode({widg.obj}, {scrollbar_mode})")
    return init


async def checkbox_to_code(var: Widget, checkbox_conf):
    """For a text object, create and set text"""
    if value := checkbox_conf.get(df.CONF_TEXT):
        return await lv_text.set_text(var, value)
    return []


async def label_to_code(var: Widget, label_conf):
    """For a text object, create and set text"""
    init = []
    if value := label_conf.get(df.CONF_TEXT):
        init.extend(await lv_text.set_text(var, value))
    init.extend(var.set_property(df.CONF_LONG_MODE, label_conf))
    init.extend(var.set_property(df.CONF_RECOLOR, label_conf))
    return init


async def textarea_to_code(var: Widget, ta_conf: dict):
    init = []
    for prop in (df.CONF_TEXT, df.CONF_PLACEHOLDER_TEXT, df.CONF_ACCEPTED_CHARS):
        if value := ta_conf.get(prop):
            init.extend(await lv_text.set_text(var, value, prop))
    init.extend(
        var.set_property(
            CONF_MAX_LENGTH, await lv_int.process(ta_conf.get(CONF_MAX_LENGTH))
        )
    )
    init.extend(
        var.set_property(
            df.CONF_PASSWORD_MODE,
            await lv_bool.process(ta_conf.get(df.CONF_PASSWORD_MODE)),
        )
    )
    init.extend(
        var.set_property(
            df.CONF_ONE_LINE, await lv_bool.process(ta_conf.get(df.CONF_ONE_LINE))
        )
    )
    return init


async def keyboard_to_code(var: Widget, kb_conf: dict):
    init = []
    init.extend(var.set_property(CONF_MODE, kb_conf))
    if ta := kb_conf.get(df.CONF_TEXTAREA):
        ta = await get_widget(ta)
        init.extend(var.set_property(df.CONF_TEXTAREA, ta.obj))
    return init


async def obj_to_code(_, __):
    return []


async def tileview_to_code(var: Widget, config: dict):
    init = []
    for widg in config[df.CONF_TILES]:
        w_type, wc = next(iter(widg.items()))
        w_id = wc[df.CONF_TILE_ID]
        tile_obj = cg.Pvariable(w_id, cg.nullptr, type_=ty.lv_obj_t)
        tile = Widget(tile_obj, ty.lv_tile_t)
        widget_map[w_id] = tile
        dirs = wc[df.CONF_DIR]
        if isinstance(dirs, list):
            dirs = "|".join(dirs)
        init.append(
            f"{tile.obj} = lv_tileview_add_tile({var.obj}, {wc[df.CONF_COLUMN]}, {wc[CONF_ROW]}, {dirs})"
        )
        ext_init = await widget_to_code(wc, w_type, tile)
        init.extend(ext_init)
    return init


async def page_to_code(config, pconf, index):
    init = []
    id = pconf[CONF_ID]
    var = cg.new_Pvariable(id)
    page = Widget(var, ty.lv_obj_t, config, f"{var}->page")
    widget_map[id] = page
    init.append(f"{page.var}->index = {index}")
    init.append(f"{page.obj} = lv_obj_create(nullptr)")
    skip = pconf[df.CONF_SKIP]
    init.append(f"{var}->skip = {skip}")
    # Set outer config first
    init.extend(await set_obj_properties(page, config))
    init.extend(await set_obj_properties(page, pconf))
    if df.CONF_WIDGETS in pconf:
        for widg in pconf[df.CONF_WIDGETS]:
            w_type, w_cnfig = next(iter(widg.items()))
            ext_init = await widget_to_code(w_cnfig, w_type, page)
            init.extend(ext_init)
    return var, init


async def switch_to_code(var, btn):
    return []


async def btn_to_code(var, btn):
    return []


async def led_to_code(var: Widget, config):
    init = []
    init.extend(
        var.set_property(CONF_COLOR, await lv_color.process(config[CONF_COLOR]))
    )
    init.extend(
        var.set_property(
            CONF_BRIGHTNESS, await lv_brightness.process(config.get(CONF_BRIGHTNESS))
        )
    )
    return init


SHOW_SCHEMA = LVGL_SCHEMA.extend(
    {
        cv.Optional(df.CONF_ANIMATION, default="NONE"): df.LV_ANIM.one_of,
        cv.Optional(CONF_TIME, default="50ms"): lv_milliseconds,
    }
)


def tile_select_validate(config):
    row = CONF_ROW in config
    column = df.CONF_COLUMN in config
    tile = df.CONF_TILE_ID in config
    if tile and (row or column) or not tile and not (row and column):
        raise cv.Invalid("Specify either a tile id, or both a row and a column")
    return config


@automation.register_action(
    "lvgl.tileview.select",
    ty.ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_tileview_t),
            cv.Optional(df.CONF_ANIMATED, default=False): lv.animated,
            cv.Optional(CONF_ROW): lv_int,
            cv.Optional(df.CONF_COLUMN): lv_int,
            cv.Optional(df.CONF_TILE_ID): cv.use_id(ty.lv_tile_t),
        },
    ).add_extra(tile_select_validate),
)
async def tileview_select(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    if tile := config.get(df.CONF_TILE_ID):
        tile = await cg.get_variable(tile)
        init = [f"lv_obj_set_tile({widget.obj}, {tile}, {config[df.CONF_ANIMATED]})"]
    else:
        init = [
            f"lv_obj_set_tile_id({widget.obj}, {config[df.CONF_COLUMN]}, {config[CONF_ROW]}, {config[df.CONF_ANIMATED]})"
        ]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.spinbox.increment",
    ty.ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_spinbox_t),
        },
        key=CONF_ID,
    ),
)
async def spinbox_increment(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = [f"lv_spinbox_increment({widget.obj})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.spinbox.decrement",
    ty.ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_spinbox_t),
        },
        key=CONF_ID,
    ),
)
async def spinbox_decrement(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = [f"lv_spinbox_decrement({widget.obj})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.animimg.start",
    ty.ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_animimg_t),
        },
        key=CONF_ID,
    ),
)
async def animimg_start(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = [f"lv_animimg_start({widget.obj})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.animimg.stop",
    ty.ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_animimg_t),
        },
        key=CONF_ID,
    ),
)
async def animimg_stop(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = [f"lv_animimg_stop({widget.obj})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.animimg.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_ANIMIMG),
)
async def animimg_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = await animimg_to_code(widget, config)
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.page.next",
    ty.ObjUpdateAction,
    SHOW_SCHEMA,
)
async def page_next_to_code(config, action_id, template_arg, args):
    lv_comp = await get_widget(config[df.CONF_LVGL_ID])
    animation = config[df.CONF_ANIMATION]
    time = await lv_milliseconds.process(config[CONF_TIME])
    init = [f"{lv_comp.obj}->show_next_page(false, {animation}, {time})"]
    return await action_to_code(init, action_id, lv_comp, template_arg, args)


@automation.register_action(
    "lvgl.page.previous",
    ty.ObjUpdateAction,
    SHOW_SCHEMA,
)
async def page_previous_to_code(config, action_id, template_arg, args):
    lv_comp = await get_widget(config[df.CONF_LVGL_ID])
    animation = config[df.CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp.obj}->show_next_page(true, {animation}, {time})"]
    return await action_to_code(init, action_id, lv_comp, template_arg, args)


@automation.register_action(
    "lvgl.page.show",
    ty.ObjUpdateAction,
    cv.maybe_simple_value(
        SHOW_SCHEMA.extend(
            {
                cv.Required(CONF_ID): cv.use_id(ty.lv_page_t),
            }
        ),
        key=CONF_ID,
    ),
)
async def page_show_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    lv_comp = await cg.get_variable(config[df.CONF_LVGL_ID])
    animation = config[df.CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp}->show_page({widget.var}->index, {animation}, {time})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.led.update",
    ty.ObjUpdateAction,
    modify_schema(CONF_LED),
)
async def led_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await led_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


async def roller_to_code(roller, config):
    lv.lv_uses.add("label")
    init = []
    mode = config[CONF_MODE]
    if options := config.get(CONF_OPTIONS):
        text = cg.safe_exp("\n".join(options))
        init.append(f"lv_roller_set_options({roller.obj}, {text}, {mode})")
    animated = config.get(df.CONF_ANIMATED) or "LV_ANIM_OFF"
    if selected := config.get(df.CONF_SELECTED_INDEX):
        value = await lv_int.process(selected)
        init.extend(roller.set_property("selected", value, animated))
    init.extend(
        roller.set_property(
            df.CONF_VISIBLE_ROW_COUNT,
            await lv_int.process(config.get(df.CONF_VISIBLE_ROW_COUNT)),
        )
    )
    return init


@automation.register_action(
    "lvgl.roller.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_ROLLER),
)
async def roller_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await roller_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


async def dropdown_to_code(dropdown: Widget, config):
    lv.lv_uses.add("label")
    obj = dropdown.obj
    init = []
    if options := config.get(CONF_OPTIONS):
        text = cg.safe_exp("\n".join(options))
        init.extend(dropdown.set_property("options", text))
    if symbol := config.get(df.CONF_SYMBOL):
        init.extend(dropdown.set_property("symbol", await lv_text.process(symbol)))
    if selected := config.get(df.CONF_SELECTED_INDEX):
        value = await lv_int.process(selected)
        init.extend(dropdown.set_property("selected", value))
    if dir := config.get(df.CONF_DIR):
        init.extend(dropdown.set_property("dir", dir))
    if list := config.get(df.CONF_DROPDOWN_LIST):
        s = Widget(dropdown, ty.lv_dropdown_list_t, list, f"{dropdown.obj}__list")
        init.extend(add_temp_var("lv_obj_t", s.obj))
        init.append(f"{s.obj} = lv_dropdown_get_list({obj});")
        init.extend(await set_obj_properties(s, list))
    return init


@automation.register_action(
    "lvgl.dropdown.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_DROPDOWN),
)
async def dropdown_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await dropdown_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


async def get_button_data(config, id, btnm: Widget):
    """
    Process a button matrix button list
    :param config: The row list
    :param id: An id basis for the text array
    :param btnm: The parent variable
    :return: text array id, control list, width list
    """
    text_list = []
    ctrl_list = []
    width_list = []
    key_list = []
    btn_id_list = []
    for row in config:
        for btnconf in row.get(df.CONF_BUTTONS) or ():
            bid = btnconf[CONF_ID]
            index = len(width_list)
            widget = MatrixButton(btnm, ty.LvBtnmBtn, btnconf, index)
            widget_map[bid] = widget
            btn_id_list.append(cg.new_Pvariable(bid, index))
            if text := btnconf.get(df.CONF_TEXT):
                text_list.append(f"{cg.safe_exp(text)}")
            else:
                text_list.append("")
            key_list.append(btnconf.get(df.CONF_KEY_CODE) or 0)
            width_list.append(btnconf[CONF_WIDTH])
            ctrl = ["(int)LV_BTNMATRIX_CTRL_CLICK_TRIG"]
            if controls := btnconf.get(df.CONF_CONTROL):
                for item in controls:
                    ctrl.extend(
                        [
                            f"(int)LV_BTNMATRIX_CTRL_{k.upper()}"
                            for k, v in item.items()
                            if v
                        ]
                    )
            ctrl_list.append("|".join(ctrl))
        text_list.append('"\\n"')
    text_list = text_list[:-1]
    text_list.append("NULL")
    text_id = ID(f"{id.id}_text_array", is_declaration=True, type=ty.char_ptr)
    text_id = cg.static_const_array(
        text_id, cg.RawExpression("{" + ",".join(text_list) + "}")
    )
    return text_id, ctrl_list, width_list, key_list, btn_id_list


def set_btn_data(btnm: Widget, ctrl_list, width_list):
    init = []
    for index, ctrl in enumerate(ctrl_list):
        init.append(f"lv_btnmatrix_set_btn_ctrl({btnm.obj}, {index}, {ctrl})")
    for index, width in enumerate(width_list):
        init.append(f"lv_btnmatrix_set_btn_width({btnm.obj}, {index}, {width})")
    return init


async def btnmatrix_to_code(btnm: Widget, conf):
    id = conf[CONF_ID]
    text_id, ctrl_list, width_list, key_list, btn_id_list = await get_button_data(
        conf[df.CONF_ROWS], id, btnm
    )
    init = [f"lv_btnmatrix_set_map({btnm.obj}, {text_id})"]
    init.extend(set_btn_data(btnm, ctrl_list, width_list))
    init.append(
        f"lv_btnmatrix_set_one_checked({btnm.obj}, {conf[df.CONF_ONE_CHECKED]})"
    )
    for index, key in enumerate(key_list):
        if key != 0:
            init.append(f"{btnm.var}->set_key({index}, {key})")
    for bid in btn_id_list:
        init.append(f"{btnm.var}->add_btn({bid})")
    return init


async def msgbox_to_code(conf):
    """
    Construct a message box. This consists of a full-screen translucent background enclosing a centered container
    with an optional title, body, close button and a button matrix. And any other widgets the user cares to add
    :param conf: The config data
    :return: code to add to the init lambda
    """
    lv.lv_uses.add("FLEX")
    lv.lv_uses.add("btnmatrix")
    lv.lv_uses.add("label")
    init = []
    id = conf[CONF_ID]
    outer = cg.new_variable(
        ID(id.id, is_declaration=True, type=ty.lv_obj_t_ptr), cg.nullptr
    )
    btnm = cg.new_variable(
        ID(f"{id.id}_btnm", is_declaration=True, type=ty.lv_obj_t_ptr), cg.nullptr
    )
    msgbox = cg.new_variable(
        ID(f"{id.id}_msgbox", is_declaration=True, type=ty.lv_obj_t_ptr), cg.nullptr
    )
    btnm_widg = Widget(btnm, ty.lv_btnmatrix_t)
    widget_map[id] = btnm_widg
    text_id, ctrl_list, width_list, _, _ = await get_button_data((conf,), id, btnm_widg)
    text = await lv_text.process(conf.get(df.CONF_BODY))
    title = await lv_text.process(conf.get(df.CONF_TITLE))
    close_button = conf[df.CONF_CLOSE_BUTTON]
    init.append(
        f"""{outer} = lv_obj_create(lv_disp_get_layer_top(lv_disp));
                    lv_obj_set_width({outer}, lv_pct(100));
                    lv_obj_set_height({outer}, lv_pct(100));
                    lv_obj_set_style_bg_opa({outer}, 128, 0);
                    lv_obj_set_style_bg_color({outer}, lv_color_black(), 0);
                    lv_obj_set_style_border_width({outer}, 0, 0);
                    lv_obj_set_style_pad_all({outer}, 0, 0);
                    lv_obj_set_style_radius({outer}, 0, 0);
                    lv_obj_add_flag({outer}, LV_OBJ_FLAG_HIDDEN);
                    {msgbox} = lv_msgbox_create({outer}, {title}, {text}, {text_id}, {close_button});
                    lv_obj_set_style_align({msgbox}, LV_ALIGN_CENTER, 0);
                    {btnm} = lv_msgbox_get_btns({msgbox});
                """
    )
    if close_button:
        init.append(
            f"""lv_obj_remove_event_cb(lv_msgbox_get_close_btn({msgbox}), nullptr);
                        lv_obj_add_event_cb(lv_msgbox_get_close_btn({msgbox}), [] (lv_event_t *ev) {{
                            lv_obj_add_flag({outer}, LV_OBJ_FLAG_HIDDEN);
                        }}, LV_EVENT_CLICKED, nullptr);
                    """
        )
    if len(ctrl_list) != 0 or len(width_list) != 0:
        s = f"{msgbox}__tobj"
        init.extend(add_temp_var("lv_obj_t", s))
        init.append(f"{s} = lv_msgbox_get_btns({msgbox})")
        init.extend(set_btn_data(Widget(s, ty.lv_obj_t), ctrl_list, width_list))
    return init


async def spinbox_to_code(widget: Widget, config):
    init = []
    lv.lv_uses.add("TEXTAREA")
    lv.lv_uses.add("LABEL")
    digits = config[df.CONF_DIGITS]
    scale = 10 ** config[df.CONF_DECIMAL_PLACES]
    range_from = int(config[CONF_RANGE_FROM])
    range_to = int(config[CONF_RANGE_TO])
    step = int(config[CONF_STEP])
    widget.scale = scale
    widget.step = step
    widget.range_to = range_to
    widget.range_from = range_from
    init.append(
        f"lv_spinbox_set_range({widget.obj}, {range_from * scale}, {range_to * scale})"
    )
    if value := config.get(CONF_VALUE):
        init.extend(widget.set_value(await lv_float.process(value)))
    init.extend(widget.set_property(CONF_STEP, step * scale))
    init.extend(widget.set_property(df.CONF_ROLLOVER, config))
    init.append(
        f"lv_spinbox_set_digit_format({widget.obj}, {digits}, {digits - config[df.CONF_DECIMAL_PLACES]})"
    )
    return init


async def animimg_to_code(var: Widget, config):
    lv.lv_uses.add("label")
    lv.lv_uses.add("img")
    init = []
    wid = config[CONF_ID]
    if df.CONF_SRC in config:
        srcs = (
            "{" + ",".join([f"lv_img_from({x.id})" for x in config[df.CONF_SRC]]) + "}"
        )
        src_id = ID(f"{wid}_src", is_declaration=True, type=ty.void_ptr)
        src_arry = cg.static_const_array(src_id, cg.RawExpression(srcs))
        count = len(config[df.CONF_SRC])
        init.append(f"lv_animimg_set_src({wid}, {src_arry}, {count})")
    init.extend(var.set_property(df.CONF_REPEAT_COUNT, config))
    init.extend(var.set_property(CONF_DURATION, config))
    if config.get(df.CONF_AUTO_START):
        init.append(f"lv_animimg_start({var.obj})")
    return init


async def img_to_code(var: Widget, img):
    init = [f"lv_img_set_src({var.obj}, lv_img_from({img[df.CONF_SRC]}))"]
    if angle := img.get(CONF_ANGLE):
        pivot_x = img[df.CONF_PIVOT_X]
        pivot_y = img[df.CONF_PIVOT_Y]
        init.extend(
            [
                f"lv_img_set_pivot({var.obj}, {pivot_x}, {pivot_y})",
                f"lv_img_set_angle({var.obj}, {angle})",
            ]
        )
    if zoom := img.get(df.CONF_ZOOM):
        init.append(f"lv_img_set_zoom({var.obj}, {zoom})")
    if offset_x := img.get(df.CONF_OFFSET_X):
        init.append(f"lv_img_set_offset_x({var.obj}, {offset_x})")
    if offset_y := img.get(df.CONF_OFFSET_Y):
        init.append(f"lv_img_set_offset_y({var.obj}, {offset_y})")
    if antialias := img.get(df.CONF_ANTIALIAS):
        init.append(f"lv_img_set_antialias({var.obj}, {antialias})")
    if mode := img.get(CONF_MODE):
        init.append(f"lv_img_set_size_mode({var.obj}, {mode})")
    return init


async def line_to_code(var: Widget, line):
    """For a line object, create and add the points"""
    data = line[df.CONF_POINTS]
    point_list = data[df.CONF_POINTS]
    initialiser = cg.RawExpression(
        "{" + ",".join(map(lambda p: "{" + f"{p[0]}, {p[1]}" + "}", point_list)) + "}"
    )
    points = cg.static_const_array(data[CONF_ID], initialiser)
    return [f"lv_line_set_points({var.obj}, {points}, {len(point_list)})"]


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


async def meter_to_code(meter: Widget, meter_conf):
    """For a meter object, create and set parameters"""

    var = meter.obj
    init = []
    s = "meter_var"
    init.extend(add_temp_var("lv_meter_scale_t", s))
    for scale in meter_conf.get(df.CONF_SCALES) or ():
        rotation = 90 + (360 - scale[df.CONF_ANGLE_RANGE]) / 2
        if CONF_ROTATION in scale:
            rotation = scale[CONF_ROTATION] // 10
        init.append(f"{s} = lv_meter_add_scale({var})")
        init.append(
            f"lv_meter_set_scale_range({var}, {s}, {scale[CONF_RANGE_FROM]},"
            + f"{scale[CONF_RANGE_TO]}, {scale[df.CONF_ANGLE_RANGE]}, {rotation})",
        )
        if ticks := scale.get(df.CONF_TICKS):
            color = await lv_color.process(ticks[CONF_COLOR])
            init.append(
                f"lv_meter_set_scale_ticks({var}, {s}, {ticks[CONF_COUNT]},"
                + f"{ticks[CONF_WIDTH]}, {ticks[CONF_LENGTH]}, {color})"
            )
            if df.CONF_MAJOR in ticks:
                major = ticks[df.CONF_MAJOR]
                color = await lv_color.process(major[CONF_COLOR])
                init.append(
                    f"lv_meter_set_scale_major_ticks({var}, {s}, {major[df.CONF_STRIDE]},"
                    + f"{major[CONF_WIDTH]}, {major[CONF_LENGTH]}, {color},"
                    + f"{major[df.CONF_LABEL_GAP]})"
                )
        for indicator in scale.get(df.CONF_INDICATORS) or ():
            (t, v) = next(iter(indicator.items()))
            iid = v[CONF_ID]
            ivar = cg.new_variable(iid, cg.nullptr, type_=ty.lv_meter_indicator_t_ptr)
            # Enable getting the meter to which this belongs.
            widget_map[iid] = Widget(var, ty.get_widget_type(t), v, ivar)
            if t == df.CONF_LINE:
                color = await lv_color.process(v[CONF_COLOR])
                init.append(
                    f"{ivar} = lv_meter_add_needle_line({var}, {s}, {v[CONF_WIDTH]},"
                    + f"{color}, {v[df.CONF_R_MOD]})"
                )
            if t == df.CONF_ARC:
                color = await lv_color.process(v[CONF_COLOR])
                init.append(
                    f"{ivar} = lv_meter_add_arc({var}, {s}, {v[CONF_WIDTH]},"
                    + f"{color}, {v[df.CONF_R_MOD]})"
                )
            if t == df.CONF_TICK_STYLE:
                color_start = await lv_color.process(v[df.CONF_COLOR_START])
                color_end = await lv_color.process(
                    v.get(df.CONF_COLOR_END) or color_start
                )
                init.append(
                    f"{ivar} = lv_meter_add_scale_lines({var}, {s}, {color_start},"
                    + f"{color_end}, {v[CONF_LOCAL]}, {v[CONF_WIDTH]})"
                )
            if t == df.CONF_IMG:
                lv.lv_uses.add("img")
                init.append(
                    f"{ivar} = lv_meter_add_needle_img({var}, {s}, lv_img_from({v[df.CONF_SRC]}),"
                    + f"{v[df.CONF_PIVOT_X]}, {v[df.CONF_PIVOT_Y]})"
                )
            start_value = await get_start_value(v)
            end_value = await get_end_value(v)
            init.extend(set_indicator_values(var, ivar, start_value, end_value))

    return init


async def spinner_to_code(spinner: Widget, config):
    lv.lv_uses.add("arc")
    return []


async def arc_to_code(arc: Widget, config):
    var = arc.obj
    init = [
        f"lv_arc_set_range({var}, {config[CONF_MIN_VALUE]}, {config[CONF_MAX_VALUE]})",
        f"lv_arc_set_bg_angles({var}, {config[df.CONF_START_ANGLE] // 10}, {config[df.CONF_END_ANGLE] // 10})",
        f"lv_arc_set_rotation({var}, {config[CONF_ROTATION] // 10})",
        f"lv_arc_set_mode({var}, {config[CONF_MODE]})",
        f"lv_arc_set_change_rate({var}, {config[df.CONF_CHANGE_RATE]})",
    ]
    if not config[df.CONF_ADJUSTABLE]:
        init.extend(
            [
                f"lv_obj_remove_style({var}, nullptr, LV_PART_KNOB)",
                f"lv_obj_clear_flag({var}, LV_OBJ_FLAG_CLICKABLE)",
            ]
        )

    value = await get_start_value(config)
    if value is not None:
        init.append(f"lv_arc_set_value({var}, {value})")
    return init


async def bar_to_code(baah: Widget, conf):
    var = baah.obj
    init = [
        f"lv_bar_set_range({var}, {conf[CONF_MIN_VALUE]}, {conf[CONF_MAX_VALUE]})",
        f"lv_bar_set_mode({var}, {conf[CONF_MODE]})",
    ]
    value = await get_start_value(conf)
    if value is not None:
        init.append(f"lv_bar_set_value({var}, {value}, LV_ANIM_OFF)")
    return init


async def rotary_encoders_to_code(var, config):
    init = []
    if df.CONF_ROTARY_ENCODERS not in config:
        return init
    lv.lv_uses.add("ROTARY_ENCODER")
    for enc_conf in config[df.CONF_ROTARY_ENCODERS]:
        sensor = await cg.get_variable(enc_conf[CONF_SENSOR])
        lpt = enc_conf[df.CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = enc_conf[df.CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
        listener = cg.new_Pvariable(enc_conf[CONF_ID], lpt, lprt)
        await cg.register_parented(listener, var)
        if group := add_group(enc_conf.get(CONF_GROUP)):
            init.append(
                f"lv_indev_set_group(lv_indev_drv_register(&{listener}->drv), {group})"
            )
        else:
            init.append(f"lv_indev_drv_register(&{listener}->drv)")
        init.append(
            f"{sensor}->register_listener([](uint32_t count) {{ {listener}->set_count(count); }})",
        )
        if b_sensor := enc_conf.get(CONF_BINARY_SENSOR):
            b_sensor = await cg.get_variable(b_sensor)
            init.append(
                f"{b_sensor}->add_on_state_callback([](bool state) {{ {listener}->set_pressed(state); }})"
            )
        return init


async def touchscreens_to_code(var, config):
    init = []
    if df.CONF_TOUCHSCREENS not in config:
        return init
    lv.lv_uses.add("TOUCHSCREEN")
    for touchconf in config[df.CONF_TOUCHSCREENS]:
        touchscreen = await cg.get_variable(touchconf[CONF_TOUCHSCREEN_ID])
        lpt = touchconf[df.CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = touchconf[df.CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
        listener = cg.new_Pvariable(touchconf[CONF_ID], lpt, lprt)
        await cg.register_parented(listener, var)
        init.extend(
            [
                f"lv_indev_drv_register(&{listener}->drv)",
                f"{touchscreen}->register_listener({listener})",
            ]
        )
    return init


async def generate_triggers(lv_component):
    init = []
    for widget in widget_map.values():
        if widget.config:
            obj = widget.obj
            for event, conf in {
                event: conf
                for event, conf in widget.config.items()
                if event in df.LV_EVENT_TRIGGERS
            }.items():
                event = df.LV_EVENT[event[3:].upper()]
                conf = conf[0]
                trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
                args = widget.get_args()
                value = widget.get_value()
                await automation.build_automation(trigger, args, conf)
                init.extend(widget.add_flag("LV_OBJ_FLAG_CLICKABLE"))
                init.extend(
                    widget.set_event_cb(
                        f"{trigger}->trigger({value});", f"LV_EVENT_{event.upper()}"
                    )
                )
            if on_value := widget.config.get(CONF_ON_VALUE):
                for conf in on_value:
                    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
                    args = widget.get_args()
                    value = widget.get_value()
                    await automation.build_automation(trigger, args, conf)
                    init.extend(
                        widget.set_event_cb(
                            f"{trigger}->trigger({value})",
                            "LV_EVENT_VALUE_CHANGED",
                            f"{lv_component}->get_custom_change_event()",
                        )
                    )
            if align_to := widget.config.get(df.CONF_ALIGN_TO):
                target = widget_map[align_to[CONF_ID]].obj
                align = align_to[df.CONF_ALIGN]
                x = align_to[df.CONF_X]
                y = align_to[df.CONF_Y]
                init.append(f"lv_obj_align_to({obj}, {target}, {align}, {x}, {y})")

    return init


async def disp_update(disp, config: dict):
    init = []
    if bg_color := config.get(df.CONF_DISP_BG_COLOR):
        init.append(f"lv_disp_set_bg_color({disp}, {await lv_color.process(bg_color)})")
    if bg_image := config.get(df.CONF_DISP_BG_IMAGE):
        init.append(f"lv_disp_set_bg_image({disp}, lv_img_from({bg_image}))")
    return init


async def to_code(config):
    cg.add_library("lvgl/lvgl", "8.4.0")
    add_define("USE_LVGL", "1")
    # suppress default enabling of extra widgets
    add_define("LV_CONF_SKIP", "1")
    add_define("_LV_KCONFIG_PRESENT")
    # Always enable - lots of things use it.
    add_define("LV_DRAW_COMPLEX", "1")
    add_define("_STRINGIFY(x)", "_STRINGIFY_(x)")
    add_define("_STRINGIFY_(x)", "#x")
    add_define("LV_TICK_CUSTOM", "1")
    add_define(
        "LV_TICK_CUSTOM_INCLUDE", "_STRINGIFY(esphome/components/lvgl/lvgl_hal.h)"
    )
    add_define("LV_TICK_CUSTOM_SYS_TIME_EXPR", "(lv_millis())")
    add_define("LV_MEM_CUSTOM", "1")
    add_define("LV_MEM_CUSTOM_ALLOC", "lv_custom_mem_alloc")
    add_define("LV_MEM_CUSTOM_FREE", "lv_custom_mem_free")
    add_define("LV_MEM_CUSTOM_REALLOC", "lv_custom_mem_realloc")
    add_define(
        "LV_MEM_CUSTOM_INCLUDE", "_STRINGIFY(esphome/components/lvgl/lvgl_hal.h)"
    )

    add_define("LV_LOG_LEVEL", f"LV_LOG_LEVEL_{config[df.CONF_LOG_LEVEL]}")
    for font in lv.lv_fonts_used:
        add_define(f"LV_FONT_{font.upper()}")
    add_define("LV_COLOR_DEPTH", config[df.CONF_COLOR_DEPTH])
    default_font = config[df.CONF_DEFAULT_FONT]
    add_define("LV_FONT_DEFAULT", default_font)
    if lv.is_esphome_font(default_font):
        add_define("LV_FONT_CUSTOM_DECLARE", f"LV_FONT_DECLARE(*{default_font})")

    if config[df.CONF_COLOR_DEPTH] == 16:
        add_define(
            "LV_COLOR_16_SWAP",
            "1" if config[df.CONF_BYTE_ORDER] == "big_endian" else "0",
        )
    add_define(
        "LV_COLOR_CHROMA_KEY", await lv_color.process(config[df.CONF_TRANSPARENCY_KEY])
    )
    CORE.add_build_flag("-Isrc")

    cg.add_global(ty.lvgl_ns.using)
    lv_component = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(lv_component, config)
    widget_map[config[CONF_ID]] = Widget(lv_component, ty.LvglComponent, config)
    displays = set()
    if display := config.get(CONF_DISPLAY_ID):
        displays.add(display)
    for display in config.get(df.CONF_DISPLAYS, []):
        displays.add(display[CONF_DISPLAY_ID])
    for display in displays:
        cg.add(lv_component.add_display(await cg.get_variable(display)))
    frac = config[CONF_BUFFER_SIZE]
    if frac >= 0.75:
        frac = 1
    elif frac >= 0.375:
        frac = 2
    elif frac > 0.19:
        frac = 4
    else:
        frac = 8
    cg.add(lv_component.set_buffer_frac(int(frac)))
    cg.add(lv_component.set_full_refresh(config[df.CONF_FULL_REFRESH]))
    cgen("lv_init()")
    if df.CONF_ROTARY_ENCODERS in config:  # or df.CONF_KEYBOARDS in config
        cgen("lv_group_set_default(lv_group_create())")
    init = []
    if lv.esphome_fonts_used:
        add_define("USE_FONT", "1")
        for font in lv.esphome_fonts_used:
            getter = cg.RawExpression(f"(new lvgl::FontEngine({font}))->get_lv_font()")
            cg.Pvariable(
                ID(f"{font}_as_lv_font_", True, ty.lv_font_t.operator("const")), getter
            )
    if style_defs := config.get(df.CONF_STYLE_DEFINITIONS, []):
        await styles_to_code(style_defs)
    if theme := config.get(df.CONF_THEME):
        lv.lv_uses.add("THEME")
        await theme_to_code(theme)
    if msgboxes := config.get(df.CONF_MSGBOXES):
        lv.lv_uses.add("MSGBOX")
        for msgbox in msgboxes:
            init.extend(await msgbox_to_code(msgbox))
    lv_scr_act = Widget("lv_scr_act()", ty.lv_obj_t, config, "lv_scr_act()")
    if top_conf := config.get(df.CONF_TOP_LAYER):
        top_layer = Widget("lv_disp_get_layer_top(lv_disp)", ty.lv_obj_t)
        init.extend(await set_obj_properties(top_layer, top_conf))
        if widgets := top_conf.get(df.CONF_WIDGETS):
            for widg in widgets:
                w_type, w_cnfig = next(iter(widg.items()))
                ext_init = await widget_to_code(w_cnfig, w_type, top_layer)
                init.extend(ext_init)
    if widgets := config.get(df.CONF_WIDGETS):
        init.extend(await set_obj_properties(lv_scr_act, config))
        for widg in widgets:
            w_type, w_cnfig = next(iter(widg.items()))
            ext_init = await widget_to_code(w_cnfig, w_type, lv_scr_act)
            init.extend(ext_init)
    if pages := config.get(CONF_PAGES):
        for index, pconf in enumerate(pages):
            pvar, pinit = await page_to_code(config, pconf, index)
            init.append(f"{lv_component}->add_page({pvar})")
            init.extend(pinit)

    global widgets_completed
    widgets_completed = True
    init.append(f"{lv_component}->set_page_wrap({config[df.CONF_PAGE_WRAP]})")
    init.extend(await generate_triggers(lv_component))
    init.extend(await touchscreens_to_code(lv_component, config))
    init.extend(await rotary_encoders_to_code(lv_component, config))
    if on_idle := config.get(CONF_ON_IDLE):
        for conf in on_idle:
            templ = await cg.templatable(conf[CONF_TIMEOUT], [], cg.uint32)
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], lv_component, templ)
            await automation.build_automation(trigger, [], conf)

    init.extend(await disp_update("lv_disp", config))
    await add_init_lambda(lv_component, init)
    for use in lv.lv_uses:
        CORE.add_build_flag(f"-DLV_USE_{use.upper()}=1")
    for comp in lv.lvgl_components_required:
        add_define(f"LVGL_USES_{comp.upper()}")
    # These must be build flags, since the lvgl code does not read our defines.h
    for macro, value in lv_defines.items():
        cg.add_build_flag(f"-D\\'{macro}\\'=\\'{value}\\'")


def indicator_update_schema(base):
    return base.extend({cv.Required(CONF_ID): cv.use_id(ty.lv_meter_indicator_t)})


async def action_to_code(action, action_id, widg: Widget, template_arg, args):
    if nc := widg.check_null():
        action.insert(0, nc)
    lamb = await cg.process_lambda(Lambda(";\n".join([*action, ""])), args)
    var = cg.new_Pvariable(action_id, template_arg, lamb)
    return var


async def update_to_code(config, action_id, widget: Widget, init, template_arg, args):
    if config is not None:
        init.extend(await set_obj_properties(widget, config))
        if (
            widget.type.value_property is not None
            and widget.type.value_property in config
        ):
            init.append(
                f"""
                lv_event_send({widget.obj}, LV_EVENT_VALUE_CHANGED, nullptr);
                        """
            )
    return await action_to_code(init, action_id, widget, template_arg, args)


DISP_BG_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_DISP_BG_IMAGE): cv.use_id(Image_),
        cv.Optional(df.CONF_DISP_BG_COLOR): lv_color,
    }
)

CONFIG_SCHEMA = (
    cv.polling_component_schema("1s")
    .extend(obj_schema("obj"))
    .extend(
        {
            cv.Optional(CONF_ID, default=df.CONF_LVGL_COMPONENT): cv.declare_id(
                ty.LvglComponent
            ),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(Display),
            cv.Optional(df.CONF_DISPLAYS): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_DISPLAY_ID): cv.use_id(Display),
                    },
                    key=CONF_DISPLAY_ID,
                ),
            ),
            cv.Optional(df.CONF_TOUCHSCREENS): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
                        cv.Optional(
                            df.CONF_LONG_PRESS_TIME, default="400ms"
                        ): cv.positive_time_period_milliseconds,
                        cv.Optional(
                            df.CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
                        ): cv.positive_time_period_milliseconds,
                        cv.GenerateID(): cv.declare_id(ty.LVTouchListener),
                    },
                    key=CONF_TOUCHSCREEN_ID,
                )
            ),
            cv.Optional(df.CONF_ROTARY_ENCODERS): cv.All(
                cv.ensure_list(
                    cv.Schema(
                        {
                            cv.Required(CONF_SENSOR): cv.use_id(RotaryEncoderSensor),
                            cv.Optional(
                                df.CONF_LONG_PRESS_TIME, default="400ms"
                            ): cv.positive_time_period_milliseconds,
                            cv.Optional(
                                df.CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
                            ): cv.positive_time_period_milliseconds,
                            cv.Optional(CONF_BINARY_SENSOR): cv.use_id(BinarySensor),
                            cv.Optional(CONF_GROUP): lv.id_name,
                            cv.GenerateID(): cv.declare_id(ty.LVRotaryEncoderListener),
                        }
                    )
                ),
            ),
            cv.Optional(df.CONF_COLOR_DEPTH, default=16): cv.one_of(1, 8, 16, 32),
            cv.Optional(df.CONF_DEFAULT_FONT, default="montserrat_14"): lv.font,
            cv.Optional(df.CONF_FULL_REFRESH, default=False): cv.boolean,
            cv.Optional(CONF_BUFFER_SIZE, default="100%"): cv.percentage,
            cv.Optional(df.CONF_LOG_LEVEL, default="WARN"): cv.one_of(
                *df.LOG_LEVELS, upper=True
            ),
            cv.Optional(df.CONF_BYTE_ORDER, default="big_endian"): cv.one_of(
                "big_endian", "little_endian"
            ),
            cv.Optional(df.CONF_STYLE_DEFINITIONS): cv.ensure_list(
                cv.Schema({cv.Required(CONF_ID): cv.declare_id(ty.lv_style_t)}).extend(
                    STYLE_SCHEMA
                )
            ),
            cv.Optional(CONF_ON_IDLE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ty.IdleTrigger),
                    cv.Required(CONF_TIMEOUT): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                }
            ),
            cv.Exclusive(df.CONF_WIDGETS, CONF_PAGES): cv.ensure_list(WIDGET_SCHEMA),
            cv.Exclusive(CONF_PAGES, CONF_PAGES): cv.ensure_list(
                container_schema(df.CONF_PAGE)
            ),
            cv.Optional(df.CONF_MSGBOXES): cv.ensure_list(MSGBOX_SCHEMA),
            cv.Optional(df.CONF_PAGE_WRAP, default=True): lv_bool,
            cv.Optional(df.CONF_TOP_LAYER): container_schema(df.CONF_OBJ),
            cv.Optional(df.CONF_TRANSPARENCY_KEY, default=0x000400): lv_color,
            cv.Optional(df.CONF_THEME): cv.Schema(
                {cv.Optional(w): obj_schema(w) for w in WIDGET_TYPES}
            ),
        }
    )
    .extend(DISP_BG_SCHEMA)
).add_extra(cv.has_at_least_one_key(CONF_PAGES, df.CONF_WIDGETS))


def spinner_obj_creator(parent: Widget, config: dict):
    return f"lv_spinner_create({parent.obj}, {config[df.CONF_SPIN_TIME].total_milliseconds}, {config[df.CONF_ARC_LENGTH] // 10})"


async def widget_to_code(w_cnfig, w_type, parent: Widget):
    init = []

    creator = f"{w_type}_obj_creator"
    if creator := globals().get(creator):
        creator = creator(parent, w_cnfig)
    else:
        creator = f"lv_{w_type}_create({parent.obj})"
    lv.lv_uses.add(w_type)
    id = w_cnfig[CONF_ID]
    if id.type.inherits_from(ty.LvCompound):
        var = cg.new_Pvariable(id)
        init.append(f"{var}->set_obj({creator})")
        obj = f"{var}->obj"
    else:
        var = cg.Pvariable(w_cnfig[CONF_ID], cg.nullptr, type_=ty.lv_obj_t)
        init.append(f"{var} = {creator}")
        obj = var

    widget = Widget(var, ty.get_widget_type(w_type), w_cnfig, obj, parent)
    widget_map[id] = widget
    if theme := theme_widget_map.get(w_type):
        init.append(f"{theme}({obj})")
    init.extend(await set_obj_properties(widget, w_cnfig))
    if widgets := w_cnfig.get(df.CONF_WIDGETS):
        for widg in widgets:
            sub_type, sub_config = next(iter(widg.items()))
            ext_init = await widget_to_code(sub_config, sub_type, widget)
            init.extend(ext_init)
    fun = f"{w_type}_to_code"
    if fun := globals().get(fun):
        init.extend(await fun(widget, w_cnfig))
    else:
        raise cv.Invalid(f"No handler for widget {w_type}")
    return init


ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.Required(CONF_ID): cv.use_id(ty.lv_pseudo_button_t),
    },
    key=CONF_ID,
)


@automation.register_action("lvgl.widget.disable", ty.ObjUpdateAction, ACTION_SCHEMA)
async def obj_disable_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.add_state("LV_STATE_DISABLED")
    return await action_to_code(action, action_id, widget, template_arg, args)


@automation.register_action("lvgl.widget.enable", ty.ObjUpdateAction, ACTION_SCHEMA)
async def obj_enable_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.clear_state("LV_STATE_DISABLED")
    return await action_to_code(action, action_id, widget, template_arg, args)


@automation.register_action("lvgl.widget.show", ty.ObjUpdateAction, ACTION_SCHEMA)
async def obj_show_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.clear_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(action, action_id, widget, template_arg, args)


@automation.register_action("lvgl.widget.hide", ty.ObjUpdateAction, ACTION_SCHEMA)
async def obj_hide_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.add_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(action, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.update",
    ty.LvglAction,
    DISP_BG_SCHEMA.extend(
        {
            cv.GenerateID(): cv.use_id(ty.LvglComponent),
        }
    ),
)
async def lvgl_update_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    action = await disp_update("lvgl_comp->get_disp()", config)
    lamb = await cg.process_lambda(
        Lambda(";\n".join([*action, ""])),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.widget.update", ty.ObjUpdateAction, modify_schema(df.CONF_OBJ)
)
async def obj_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    return await update_to_code(config, action_id, widget, [], template_arg, args)


@automation.register_action(
    "lvgl.spinbox.update",
    ty.ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_spinbox_t),
            cv.Required(CONF_VALUE): lv_float,
        }
    ),
)
async def spinbox_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = widget.set_value(await lv_float.process(config[CONF_VALUE]))
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.checkbox.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_CHECKBOX),
)
async def checkbox_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = await checkbox_to_code(widget, config)
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.textarea.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_TEXTAREA),
)
async def textarea_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await textarea_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


@automation.register_action(
    "lvgl.keyboard.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_KEYBOARD),
)
async def keyboard_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await keyboard_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


@automation.register_action(
    "lvgl.label.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_LABEL),
)
async def label_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await label_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


@automation.register_action(
    "lvgl.indicator.update",
    ty.ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ty.lv_meter_indicator_t),
            cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
            cv.Exclusive(df.CONF_START_VALUE, CONF_VALUE): lv_float,
            cv.Optional(df.CONF_END_VALUE): lv_float,
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


@automation.register_action(
    "lvgl.button.update",
    ty.ObjUpdateAction,
    cv.Schema(
        {
            cv.Optional(CONF_WIDTH): cv.positive_int,
            cv.Optional(df.CONF_CONTROL): cv.ensure_list(
                cv.Schema(
                    {cv.Optional(k.lower()): cv.boolean for k in df.BTNMATRIX_CTRLS}
                ),
            ),
            cv.Required(CONF_ID): cv.use_id(ty.LvBtnmBtn),
            cv.Optional(df.CONF_SELECTED): lv_bool,
        }
    ),
)
async def button_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    assert isinstance(widget, MatrixButton)
    init = []
    if (width := config.get(CONF_WIDTH)) is not None:
        init.extend(widget.set_width(width))
    if config.get(df.CONF_SELECTED):
        init.extend(widget.set_selected())
    if controls := config.get(df.CONF_CONTROL):
        adds = []
        clrs = []
        for item in controls:
            adds.extend([k for k, v in item.items() if v])
            clrs.extend([k for k, v in item.items() if not v])
        if adds:
            init.extend(widget.set_ctrls(*adds))
        if clrs:
            init.extend(widget.clear_ctrls(*clrs))
    return await action_to_code(init, action_id, widget.var, template_arg, args)


@automation.register_action(
    "lvgl.spinner.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_SPINNER),
)
async def spinner_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.btnmatrix.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_BTNMATRIX),
)
async def btnmatrix_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.arc.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_ARC),
)
async def arc_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    value = await lv_int.process(config.get(CONF_VALUE))
    init.append(f"lv_arc_set_value({widget.obj}, {value})")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.bar.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_BAR),
)
async def bar_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    value = await lv_int.process(config.get(CONF_VALUE))
    animated = config[df.CONF_ANIMATED]
    init.append(f"lv_bar_set_value({widget.obj}, {value}, {animated})")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


async def slider_to_code(slider: Widget, config):
    lv.lv_uses.add("bar")
    var = slider.obj
    init = [
        f"lv_slider_set_range({var}, {config[CONF_MIN_VALUE]}, {config[CONF_MAX_VALUE]})",
        f"lv_slider_set_mode({var}, {config[CONF_MODE]})",
    ]
    value = await get_start_value(config)
    if value is not None:
        init.append(f"lv_slider_set_value({var}, {value}, LV_ANIM_OFF)")
    return init


@automation.register_action(
    "lvgl.slider.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_SLIDER),
)
async def slider_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    animated = config[df.CONF_ANIMATED]
    value = await lv_int.process(config.get(CONF_VALUE))
    init.append(f"lv_slider_set_value({widget.obj}, {value}, {animated})")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.img.update",
    ty.ObjUpdateAction,
    modify_schema(df.CONF_IMG),
)
async def img_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    if src := config.get(df.CONF_SRC):
        init.append(f"lv_img_set_src({widget.obj}, lv_img_from({src}))")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.widget.redraw",
    ty.LvglAction,
    cv.Schema(
        {
            cv.Optional(CONF_ID): cv.use_id(ty.lv_obj_t),
            cv.GenerateID(df.CONF_LVGL_ID): cv.use_id(ty.LvglComponent),
        }
    ),
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[df.CONF_LVGL_ID])
    if obj_id := config.get(CONF_ID):
        obj = cg.get_variable(obj_id)
    else:
        obj = "lv_scr_act()"
    lamb = await cg.process_lambda(
        Lambda(f"lv_obj_invalidate({obj});"), [(ty.LvglComponentPtr, "lvgl_comp")]
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.pause",
    ty.LvglAction,
    {
        cv.GenerateID(): cv.use_id(ty.LvglComponent),
        cv.Optional(df.CONF_SHOW_SNOW, default="false"): lv_bool,
    },
)
async def pause_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    lamb = await cg.process_lambda(
        Lambda(f"lvgl_comp->set_paused(true, {config[df.CONF_SHOW_SNOW]});"),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.resume",
    ty.LvglAction,
    {
        cv.GenerateID(): cv.use_id(ty.LvglComponent),
    },
)
async def resume_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    lamb = await cg.process_lambda(
        Lambda("lvgl_comp->set_paused(false, false);"),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_condition(
    "lvgl.is_idle",
    ty.LvglCondition,
    LVGL_SCHEMA.extend(
        {
            cv.Required(CONF_TIMEOUT): cv.templatable(
                cv.positive_time_period_milliseconds
            )
        }
    ),
)
async def lvgl_is_idle(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    lvgl = config[df.CONF_LVGL_ID]
    timeout = await cg.templatable(config[CONF_TIMEOUT], [], cg.uint32)
    if isinstance(timeout, LambdaExpression):
        timeout = f"({timeout}())"
    else:
        timeout = timeout.total_milliseconds
    await cg.register_parented(var, lvgl)
    lamb = await cg.process_lambda(
        Lambda(f"return lvgl_comp->is_idle({timeout});"),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_condition_lambda(lamb))
    return var


@automation.register_condition(
    "lvgl.is_paused",
    ty.LvglCondition,
    LVGL_SCHEMA,
)
async def lvgl_is_paused(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    lvgl = config[df.CONF_LVGL_ID]
    await cg.register_parented(var, lvgl)
    lamb = await cg.process_lambda(
        Lambda("return lvgl_comp->is_paused();"), [(ty.LvglComponentPtr, "lvgl_comp")]
    )
    cg.add(var.set_condition_lambda(lamb))
    return var
