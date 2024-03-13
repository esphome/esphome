import functools
import re
import logging
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
from esphome.components.key_provider import KeyProvider
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
)
from esphome.cpp_generator import (
    LambdaExpression,
)
from .defines import (
    # widgets
    CONF_ANIMIMG,
    CONF_ARC,
    CONF_BAR,
    CONF_BTN,
    CONF_BTNMATRIX,
    CONF_CANVAS,
    CONF_CHECKBOX,
    CONF_DROPDOWN,
    CONF_DROPDOWN_LIST,
    CONF_IMG,
    CONF_LABEL,
    CONF_LINE,
    CONF_METER,
    CONF_ROLLER,
    CONF_SLIDER,
    CONF_SPINBOX,
    CONF_SWITCH,
    CONF_TABLE,
    CONF_TEXTAREA,
    # Parts
    CONF_MAIN,
    CONF_SCROLLBAR,
    CONF_INDICATOR,
    CONF_KNOB,
    CONF_SELECTED,
    CONF_ITEMS,
    CONF_TICKS,
    CONF_CURSOR,
    CONF_TEXTAREA_PLACEHOLDER,
    CHILD_ALIGNMENTS,
    ALIGN_ALIGNMENTS,
    ARC_MODES,
    BAR_MODES,
    LOG_LEVELS,
    STATES,
    PARTS,
    FLEX_FLOWS,
    OBJ_FLAGS,
    BTNMATRIX_CTRLS,
    # Input devices
    CONF_ROTARY_ENCODERS,
    CONF_TOUCHSCREENS,
    DIRECTIONS,
    ROLLER_MODES,
    CONF_PAGE,
    LV_ANIM,
    LV_EVENT_TRIGGERS,
    LV_LONG_MODES,
    LV_EVENT,
    CONF_SPINNER,
    LV_GRID_ALIGNMENTS,
    LV_CELL_ALIGNMENTS,
)

from .lv_validation import (
    lv_one_of,
    lv_opacity,
    lv_stop_value,
    lv_any_of,
    lv_size,
    lv_font,
    lv_angle,
    pixels_or_percent,
    lv_zoom,
    lv_animated,
    join_enums,
    lv_fonts_used,
    lv_uses,
    REQUIRED_COMPONENTS,
    lvgl_components_required,
    cv_int_list,
    lv_option_string,
    lv_id_name,
    requires_component,
    lv_key_code,
    esphome_fonts_used,
    is_esphome_font,
    lv_color_validator,
    lv_bool_validator,
)
from .widget import (
    Widget,
    add_temp_var,
    lv_temp_vars,
    MatrixButton,
)
from ..sensor import Sensor
from ..text_sensor import TextSensor
from ...helpers import cpp_string_escape

# import auto
DOMAIN = "lvgl"
DEPENDENCIES = ("display",)
AUTO_LOAD = ("key_provider",)
CODEOWNERS = ("@clydebarrow",)
LOGGER = logging.getLogger(__name__)

char_ptr_const = cg.global_ns.namespace("char").operator("ptr")
void_ptr = cg.void.operator("ptr")
lv_coord_t = cg.global_ns.namespace("lv_coord_t")
lv_event_code_t = cg.global_ns.enum("lv_event_code_t")
lvgl_ns = cg.esphome_ns.namespace("lvgl")
LvglComponent = lvgl_ns.class_("LvglComponent", cg.PollingComponent)
LvglComponentPtr = LvglComponent.operator("ptr")
LvImagePtr = Image_.operator("ptr")
LVTouchListener = lvgl_ns.class_("LVTouchListener")
LVRotaryEncoderListener = lvgl_ns.class_("LVRotaryEncoderListener")
IdleTrigger = lvgl_ns.class_("IdleTrigger", automation.Trigger.template())
FontEngine = lvgl_ns.class_("FontEngine")
ObjUpdateAction = lvgl_ns.class_("ObjUpdateAction", automation.Action)
LvglCondition = lvgl_ns.class_("LvglCondition", automation.Condition)
LvglAction = lvgl_ns.class_("LvglAction", automation.Action)
lv_lambda_t = lvgl_ns.class_("LvLambdaType")
# lv_lambda_ptr_t = lvgl_ns.class_("LvLambdaType").operator("ptr")

# Can't use the native type names here, since ESPHome munges variable names and they conflict
LvCompound = lvgl_ns.class_("LvCompound")
lv_pseudo_button_t = lvgl_ns.class_("LvPseudoButton")
LvBtnmBtn = lvgl_ns.class_("LvBtnmBtn", lv_pseudo_button_t)
lv_obj_t = cg.global_ns.class_("LvObjType", lv_pseudo_button_t)
lv_font_t = cg.global_ns.class_("LvFontType")
lv_page_t = cg.global_ns.class_("LvPageType")
lv_screen_t = cg.global_ns.class_("LvScreenType")
lv_point_t = cg.global_ns.struct("LvPointType")
lv_msgbox_t = cg.global_ns.struct("LvMsgBoxType")
lv_obj_t_ptr = lv_obj_t.operator("ptr")
lv_style_t = cg.global_ns.struct("LvStyleType")
lv_color_t = cg.global_ns.struct("LvColorType")
lv_theme_t = cg.global_ns.struct("LvThemeType")
lv_theme_t_ptr = lv_theme_t.operator("ptr")
lv_meter_indicator_t = cg.global_ns.struct("LvMeterIndicatorType")
lv_indicator_t = cg.global_ns.struct("LvMeterIndicatorType")
lv_meter_indicator_t_ptr = lv_meter_indicator_t.operator("ptr")
lv_label_t = cg.MockObjClass("LvLabelType", parents=[lv_obj_t])
lv_dropdown_list_t = cg.MockObjClass("LvDropdownListType", parents=[lv_obj_t])
lv_meter_t = cg.MockObjClass("LvMeterType", parents=[lv_obj_t])
lv_btn_t = cg.MockObjClass("LvBtnType", parents=[lv_obj_t])
lv_checkbox_t = cg.MockObjClass("LvCheckboxType", parents=[lv_obj_t])
lv_line_t = cg.MockObjClass("LvLineType", parents=[lv_obj_t])
lv_img_t = cg.MockObjClass("LvImgType", parents=[lv_obj_t])
lv_animimg_t = cg.MockObjClass("LvAnimImgType", parents=[lv_obj_t])
lv_spinbox_t = cg.MockObjClass("LvSpinBoxType", parents=[lv_obj_t])
lv_number_t = lvgl_ns.class_("LvPseudoNumber")
lv_arc_t = cg.MockObjClass("LvArcType", parents=[lv_obj_t, lv_number_t])
lv_bar_t = cg.MockObjClass("LvBarType", parents=[lv_obj_t, lv_number_t])
lv_slider_t = cg.MockObjClass("LvSliderType", parents=[lv_obj_t, lv_number_t])
lv_disp_t_ptr = cg.global_ns.struct("lv_disp_t").operator("ptr")
lv_canvas_t = cg.MockObjClass("LvCanvasType", parents=[lv_obj_t])
lv_select_t = lvgl_ns.class_("LvPseudoSelect")
lv_dropdown_t = cg.MockObjClass("LvDropdownType", parents=[lv_obj_t, lv_select_t])
lv_roller_t = cg.MockObjClass("LvRollerType", parents=[lv_obj_t, lv_select_t])
lv_led_t = cg.MockObjClass("LvLedType", parents=[lv_obj_t])
lv_switch_t = cg.MockObjClass("LvSwitchType", parents=[lv_obj_t])
lv_table_t = cg.MockObjClass("LvTableType", parents=[lv_obj_t])
lv_textarea_t = cg.MockObjClass("LvTextareaType", parents=[lv_obj_t])
lv_btnmatrix_t = cg.MockObjClass(
    "LvBtnmatrixType", parents=[lv_obj_t, KeyProvider, LvCompound]
)
# Provided for the benefit of get_widget_type
lv_spinner_t = lv_obj_t
lv_ticks_t = lv_obj_t

CONF_ACTION = "action"
CONF_ADJUSTABLE = "adjustable"
CONF_ALIGN = "align"
CONF_ALIGN_TO = "align_to"
CONF_ANGLE = "angle"
CONF_ANGLE_RANGE = "angle_range"
CONF_ANIMATED = "animated"
CONF_ANIMATION = "animation"
CONF_ANTIALIAS = "antialias"
CONF_ARC_LENGTH = "arc_length"
CONF_AUTO_START = "auto_start"
CONF_BACKGROUND_STYLE = "background_style"
CONF_DECIMAL_PLACES = "decimal_places"
CONF_DIGITS = "digits"
CONF_DISP_BG_COLOR = "disp_bg_color"
CONF_DISP_BG_IMAGE = "disp_bg_image"
CONF_BODY = "body"
CONF_BUTTONS = "buttons"
CONF_BYTE_ORDER = "byte_order"
CONF_CHANGE_RATE = "change_rate"
CONF_CLOSE_BUTTON = "close_button"
CONF_COLOR_DEPTH = "color_depth"
CONF_COLOR_END = "color_end"
CONF_COLOR_START = "color_start"
CONF_CONTROL = "control"
CONF_DEFAULT = "default"
CONF_DEFAULT_FONT = "default_font"
CONF_DIR = "dir"
CONF_DISPLAY_ID = "display_id"
CONF_DISPLAYS = "displays"
CONF_END_ANGLE = "end_angle"
CONF_END_VALUE = "end_value"
CONF_FLAGS = "flags"
CONF_FLEX_FLOW = "flex_flow"
CONF_FULL_REFRESH = "full_refresh"
CONF_GRID_CELL_ROW_POS = "grid_cell_row_pos"
CONF_GRID_CELL_COLUMN_POS = "grid_cell_column_pos"
CONF_GRID_CELL_ROW_SPAN = "grid_cell_row_span"
CONF_GRID_CELL_COLUMN_SPAN = "grid_cell_column_span"
CONF_GRID_COLUMN_ALIGN = "grid_column_align"
CONF_GRID_COLUMNS = "grid_columns"
CONF_GRID_ROW_ALIGN = "grid_row_align"
CONF_GRID_ROWS = "grid_rows"
CONF_HOME = "home"
CONF_INDICATORS = "indicators"
CONF_KEY_CODE = "key_code"
CONF_LABEL_GAP = "label_gap"
CONF_LAYOUT = "layout"
CONF_LINE_WIDTH = "line_width"
CONF_LOG_LEVEL = "log_level"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_LONG_PRESS_REPEAT_TIME = "long_press_repeat_time"
CONF_LVGL_COMPONENT = "lvgl_component"
CONF_LVGL_ID = "lvgl_id"
CONF_LONG_MODE = "long_mode"
CONF_MAJOR = "major"
CONF_MSGBOXES = "msgboxes"
CONF_OBJ = "obj"
CONF_OFFSET_X = "offset_x"
CONF_OFFSET_Y = "offset_y"
CONF_ON_IDLE = "on_idle"
CONF_ONE_CHECKED = "one_checked"
CONF_NEXT = "next"
CONF_PAGE_WRAP = "page_wrap"
CONF_PIVOT_X = "pivot_x"
CONF_PIVOT_Y = "pivot_y"
CONF_POINTS = "points"
CONF_PREVIOUS = "previous"
CONF_REPEAT_COUNT = "repeat_count"
CONF_ROWS = "rows"
CONF_R_MOD = "r_mod"
CONF_RECOLOR = "recolor"
CONF_ROLLOVER = "rollover"
CONF_SCALES = "scales"
CONF_SCALE_LINES = "scale_lines"
CONF_SCROLLBAR_MODE = "scrollbar_mode"
CONF_SELECTED_INDEX = "selected_index"
CONF_SHOW_SNOW = "show_snow"
CONF_SPIN_TIME = "spin_time"
CONF_SRC = "src"
CONF_START_ANGLE = "start_angle"
CONF_START_VALUE = "start_value"
CONF_STATES = "states"
CONF_STRIDE = "stride"
CONF_STYLE = "style"
CONF_STYLES = "styles"
CONF_STYLE_DEFINITIONS = "style_definitions"
CONF_STYLE_ID = "style_id"
CONF_SKIP = "skip"
CONF_SYMBOL = "symbol"
CONF_TEXT = "text"
CONF_TITLE = "title"
CONF_TOP_LAYER = "top_layer"
CONF_TRANSPARENCY_KEY = "transparency_key"
CONF_THEME = "theme"
CONF_WIDGET = "widget"
CONF_WIDGETS = "widgets"
CONF_X = "x"
CONF_Y = "y"
CONF_ZOOM = "zoom"

# list of widgets and the parts allowed
WIDGET_TYPES = {
    CONF_ANIMIMG: (CONF_MAIN,),
    CONF_ARC: (CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
    CONF_BTN: (CONF_MAIN,),
    CONF_BAR: (CONF_MAIN, CONF_INDICATOR),
    CONF_BTNMATRIX: (CONF_MAIN, CONF_ITEMS),
    CONF_CANVAS: (CONF_MAIN,),
    CONF_CHECKBOX: (CONF_MAIN, CONF_INDICATOR),
    CONF_DROPDOWN: (CONF_MAIN, CONF_INDICATOR),
    CONF_IMG: (CONF_MAIN,),
    CONF_INDICATOR: (),
    CONF_LABEL: (CONF_MAIN, CONF_SCROLLBAR, CONF_SELECTED),
    CONF_LED: (CONF_MAIN,),
    CONF_LINE: (CONF_MAIN,),
    CONF_DROPDOWN_LIST: (CONF_MAIN, CONF_SCROLLBAR, CONF_SELECTED),
    CONF_METER: (CONF_MAIN,),
    CONF_OBJ: (CONF_MAIN,),
    # CONF_PAGE: (CONF_MAIN,),
    CONF_ROLLER: (CONF_MAIN, CONF_SELECTED),
    CONF_SLIDER: (CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
    CONF_SPINNER: (CONF_MAIN, CONF_INDICATOR),
    CONF_SWITCH: (CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
    CONF_SPINBOX: (
        CONF_MAIN,
        CONF_SCROLLBAR,
        CONF_SELECTED,
        CONF_CURSOR,
        CONF_TEXTAREA_PLACEHOLDER,
    ),
    CONF_TABLE: (CONF_MAIN, CONF_ITEMS),
    CONF_TEXTAREA: (
        CONF_MAIN,
        CONF_SCROLLBAR,
        CONF_SELECTED,
        CONF_CURSOR,
        CONF_TEXTAREA_PLACEHOLDER,
    ),
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
        if isinstance(value, Lambda):
            return f"{await cg.process_lambda(value, args, return_type=self.rtype)}()"
        if self.idtype is not None and isinstance(value, ID):
            return f"{value}->{self.idexpr};"
        if self.retmapper is not None:
            return self.retmapper(value)
        return value


lv_color = LValidator(lv_color_validator, lv_color_t)
lv_bool = LValidator(lv_bool_validator, cg.bool_, BinarySensor, "get_state()")


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

    async def set_text(self, var: Widget, value):
        """
        Set the text property
        :param var:     The widget
        :param value:   The value to be set.
        :return: The generated code
        """

        if isinstance(value, dict):
            args = [str(x) for x in value[CONF_ARGS]]
            args_ = cg.RawExpression(",".join(args))
            format = cpp_string_escape(value[CONF_FORMAT])
            set_prop = var.set_property("text", "text__")[0]
            return [
                f"""{{
                char * text__ = nullptr;
                asprintf(&text__, {format}, {args_});
                if (text__ != nullptr) {{
                    {set_prop};
                    free(text__);
                }}
            }}"""
            ]
        return var.set_property("text", await self.process(value))


lv_text = TextValidator()
lv_float = LValidator(cv.float_, cg.float_, Sensor, "get_state()")
lv_int = LValidator(cv.int_, cg.int_, Sensor, "get_state()")

cell_alignments = lv_one_of(LV_CELL_ALIGNMENTS, prefix="LV_GRID_ALIGNMENT_")
grid_alignments = lv_one_of(LV_GRID_ALIGNMENTS, prefix="LV_GRID_ALIGNMENT_")

# List the LVGL built-in fonts that are available

STYLE_PROPS = {
    "align": lv_one_of(CHILD_ALIGNMENTS, "LV_ALIGN_"),
    "arc_opa": lv_opacity,
    "arc_color": lv_color,
    "arc_rounded": lv_bool,
    "arc_width": cv.positive_int,
    "bg_color": lv_color,
    "bg_grad_color": lv_color,
    "bg_dither_mode": lv_one_of(["NONE", "ORDERED", "ERR_DIFF"], "LV_DITHER_"),
    "bg_grad_dir": lv_one_of(["NONE", "HOR", "VER"], "LV_GRAD_DIR_"),
    "bg_grad_stop": lv_stop_value,
    "bg_img_opa": lv_opacity,
    "bg_img_recolor": lv_color,
    "bg_img_recolor_opa": lv_opacity,
    "bg_main_stop": lv_stop_value,
    "bg_opa": lv_opacity,
    "border_color": lv_color,
    "border_opa": lv_opacity,
    "border_post": cv.boolean,
    "border_side": lv_any_of(
        ["NONE", "TOP", "BOTTOM", "LEFT", "RIGHT", "INTERNAL"], "LV_BORDER_SIDE_"
    ),
    "border_width": cv.positive_int,
    "clip_corner": lv_bool,
    "grid_cell_x_align": cell_alignments,
    "grid_cell_y_align": cell_alignments,
    "grid_cell_row_pos": cv.positive_int,
    "grid_cell_column_pos": cv.positive_int,
    "grid_cell_row_span": cv.positive_int,
    "grid_cell_column_span": cv.positive_int,
    "height": lv_size,
    "img_recolor": lv_color,
    "img_recolor_opa": lv_opacity,
    "line_width": cv.positive_int,
    "line_dash_width": cv.positive_int,
    "line_dash_gap": cv.positive_int,
    "line_rounded": lv_bool,
    "line_color": lv_color,
    "opa": lv_opacity,
    "opa_layered": lv_opacity,
    "outline_color": lv_color,
    "outline_opa": lv_opacity,
    "outline_pad": lv_size,
    "outline_width": lv_size,
    "pad_all": lv_size,
    "pad_bottom": lv_size,
    "pad_column": lv_size,
    "pad_left": lv_size,
    "pad_right": lv_size,
    "pad_row": lv_size,
    "pad_top": lv_size,
    "shadow_color": lv_color,
    "shadow_ofs_x": cv.int_,
    "shadow_ofs_y": cv.int_,
    "shadow_opa": lv_opacity,
    "shadow_spread": cv.int_,
    "shadow_width": cv.positive_int,
    "text_align": lv_one_of(["LEFT", "CENTER", "RIGHT", "AUTO"], "LV_TEXT_ALIGN_"),
    "text_color": lv_color,
    "text_decor": lv_any_of(["NONE", "UNDERLINE", "STRIKETHROUGH"], "LV_TEXT_DECOR_"),
    "text_font": lv_font,
    "text_letter_space": cv.positive_int,
    "text_line_space": cv.positive_int,
    "text_opa": lv_opacity,
    "transform_angle": lv_angle,
    "transform_height": pixels_or_percent,
    "transform_pivot_x": pixels_or_percent,
    "transform_pivot_y": pixels_or_percent,
    "transform_zoom": lv_zoom,
    "translate_x": pixels_or_percent,
    "translate_y": pixels_or_percent,
    "max_height": pixels_or_percent,
    "max_width": pixels_or_percent,
    "min_height": pixels_or_percent,
    "min_width": pixels_or_percent,
    "radius": cv.Any(lv_size, lv_one_of(["CIRCLE"], "LV_RADIUS_")),
    "width": lv_size,
    "x": pixels_or_percent,
    "y": pixels_or_percent,
}


def validate_max_min(config):
    if CONF_MAX_VALUE in config and CONF_MIN_VALUE in config:
        if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
            raise cv.Invalid("max_value must be greater than min_value")
    return config


def validate_grid(config):
    if config.get(CONF_LAYOUT) == "LV_LAYOUT_GRID":
        if CONF_GRID_ROWS not in config or CONF_GRID_COLUMNS not in config:
            raise cv.Invalid("grid layout requires grid_rows and grid_columns")
    elif any((key in config) for key in list(GRID_CONTAINER_SCHEMA.keys())):
        raise cv.Invalid("grid items apply to grid layout only")
    if CONF_GRID_CELL_COLUMN_POS in config or CONF_GRID_CELL_ROW_POS in config:
        if CONF_GRID_CELL_ROW_SPAN not in config:
            config[CONF_GRID_CELL_ROW_SPAN] = 1
        if CONF_GRID_CELL_COLUMN_SPAN not in config:
            config[CONF_GRID_CELL_COLUMN_SPAN] = 1
    return config


def get_widget_type(typestr: str) -> cg.MockObjClass:
    return globals()[f"lv_{typestr}_t"]


def modify_schema(widget_type):
    lv_type = get_widget_type(widget_type)
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
    values = list(map(cv_int_list, value))
    if not functools.reduce(lambda f, v: f and len(v) == 2, values, True):
        raise cv.Invalid("Points must be a list of x,y integer pairs")
    return {
        CONF_ID: cv.declare_id(lv_point_t)(generate_id(CONF_POINTS)),
        CONF_POINTS: values,
    }


def part_schema(parts):
    if isinstance(parts, str) and parts in WIDGET_TYPES:
        parts = WIDGET_TYPES[parts]
    else:
        parts = (CONF_MAIN,)
    return cv.Schema({cv.Optional(part): STATE_SCHEMA for part in parts}).extend(
        STATE_SCHEMA
    )


def automation_schema(type: cg.MockObjClass = lv_obj_t):
    template = (
        automation.Trigger.template(cg.float_)
        if type.inherits_from(lv_number_t)
        else automation.Trigger.template()
    )
    return {
        cv.Optional(event): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(template),
            }
        )
        for event in LV_EVENT_TRIGGERS
    }


def validate_printf(value):
    # https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
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


TEXT_SCHEMA = cv.Schema(
    {
        cv.Exclusive(CONF_TEXT, CONF_TEXT): cv.Any(
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
        cv.Optional(CONF_STYLES): cv.ensure_list(cv.use_id(lv_style_t)),
        cv.Optional(CONF_SCROLLBAR_MODE): lv_one_of(
            ("OFF", "ON", "ACTIVE", "AUTO"), prefix="LV_SCROLLBAR_MODE_"
        ),
    }
)
STATE_SCHEMA = cv.Schema({cv.Optional(state): STYLE_SCHEMA for state in STATES}).extend(
    STYLE_SCHEMA
)
SET_STATE_SCHEMA = cv.Schema({cv.Optional(state): lv_bool for state in STATES})
FLAG_SCHEMA = cv.Schema({cv.Optional(flag): cv.boolean for flag in OBJ_FLAGS})
FLAG_LIST = cv.ensure_list(lv_one_of(OBJ_FLAGS, "LV_OBJ_FLAG_"))

BAR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.int_,
        cv.Optional(CONF_MAX_VALUE, default=100): cv.int_,
        cv.Optional(CONF_MODE, default="NORMAL"): lv_one_of(BAR_MODES, "LV_BAR_MODE_"),
        cv.Optional(CONF_ANIMATED, default=True): lv_animated,
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template(cg.float_)
                )
            }
        ),
    }
)

SLIDER_SCHEMA = BAR_SCHEMA

LINE_SCHEMA = {cv.Optional(CONF_POINTS): cv_point_list}


def lv_repeat_count(value):
    if isinstance(value, str) and value.lower() in ("forever", "infinite"):
        value = 0xFFFF
    return cv.positive_int(value)


SPINBOX_SCHEMA = {
    cv.Optional(CONF_VALUE): lv_float,
    cv.Required(CONF_RANGE_FROM): lv_float,
    cv.Required(CONF_RANGE_TO): lv_float,
    cv.Required(CONF_DIGITS): cv.positive_int,
    cv.Optional(CONF_STEP, default=1.0): cv.positive_float,
    cv.Optional(CONF_DECIMAL_PLACES, default=0): cv.positive_int,
    cv.Optional(CONF_ROLLOVER, default=False): lv_bool,
}

ANIMIMG_SCHEMA = {
    cv.Required(CONF_SRC): cv.ensure_list(cv.use_id(Image_)),
    cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_REPEAT_COUNT, default="forever"): lv_repeat_count,
    cv.Optional(CONF_AUTO_START, default=True): cv.boolean,
}

ANIMIMG_MODIFY_SCHEMA = {
    cv.Optional(CONF_DURATION): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_REPEAT_COUNT): lv_repeat_count,
}

IMG_SCHEMA = {
    cv.Required(CONF_SRC): cv.use_id(Image_),
    cv.Optional(CONF_PIVOT_X, default="50%"): lv_size,
    cv.Optional(CONF_PIVOT_Y, default="50%"): lv_size,
    cv.Optional(CONF_ANGLE): lv_angle,
    cv.Optional(CONF_ZOOM): lv_zoom,
    cv.Optional(CONF_OFFSET_X): lv_size,
    cv.Optional(CONF_OFFSET_Y): lv_size,
    cv.Optional(CONF_ANTIALIAS): lv_bool,
    cv.Optional(CONF_MODE): lv_one_of(("VIRTUAL", "REAL"), prefix="LV_IMG_SIZE_MODE_"),
}

# Schema for a single button in a btnmatrix
BTNM_BTN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEXT): cv.string,
        cv.Optional(CONF_KEY_CODE): lv_key_code,
        cv.GenerateID(): cv.declare_id(LvBtnmBtn),
        cv.Optional(CONF_WIDTH, default=1): cv.positive_int,
        cv.Optional(CONF_CONTROL): cv.ensure_list(
            cv.Schema({cv.Optional(k.lower()): cv.boolean for k in BTNMATRIX_CTRLS})
        ),
    }
).extend(automation_schema())

BTNMATRIX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ONE_CHECKED, default=False): lv_bool,
        cv.Required(CONF_ROWS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
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
        cv.Optional(CONF_START_ANGLE, default=135): lv_angle,
        cv.Optional(CONF_END_ANGLE, default=45): lv_angle,
        cv.Optional(CONF_ROTATION, default=0.0): lv_angle,
        cv.Optional(CONF_ADJUSTABLE, default=False): bool,
        cv.Optional(CONF_MODE, default="NORMAL"): lv_one_of(ARC_MODES, "LV_ARC_MODE_"),
        cv.Optional(CONF_CHANGE_RATE, default=720): cv.uint16_t,
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template(cg.float_)
                )
            }
        ),
    }
)

ARC_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
    }
)

SPINNER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ARC_LENGTH): lv_angle,
        cv.Required(CONF_SPIN_TIME): cv.positive_time_period_milliseconds,
    }
)

SPINNER_MODIFY_SCHEMA = cv.Schema({})

INDICATOR_LINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv_size,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_R_MOD, default=0): lv_size,
        cv.Optional(CONF_VALUE): lv_float,
    }
)
INDICATOR_IMG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PIVOT_X, default="50%"): lv_size,
        cv.Optional(CONF_PIVOT_Y, default="50%"): lv_size,
        cv.Optional(CONF_VALUE): lv_float,
    }
)
INDICATOR_ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv_size,
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_R_MOD, default=0): lv_size,
        cv.Exclusive(CONF_VALUE, CONF_VALUE): lv_float,
        cv.Exclusive(CONF_START_VALUE, CONF_VALUE): lv_float,
        cv.Optional(CONF_END_VALUE): lv_float,
    }
)
INDICATOR_TICKS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH, default=4): lv_size,
        cv.Optional(CONF_COLOR_START, default=0): lv_color,
        cv.Optional(CONF_COLOR_END): lv_color,
        cv.Optional(CONF_R_MOD, default=0): lv_size,
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
        cv.Exclusive(CONF_IMG, CONF_INDICATORS): cv.All(
            INDICATOR_IMG_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(lv_meter_indicator_t),
                }
            ),
        ),
        cv.Exclusive(CONF_ARC, CONF_INDICATORS): INDICATOR_ARC_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(lv_meter_indicator_t),
            }
        ),
        cv.Exclusive(CONF_TICKS, CONF_INDICATORS): INDICATOR_TICKS_SCHEMA.extend(
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
                cv.Optional(CONF_WIDTH, default=2): lv_size,
                cv.Optional(CONF_LENGTH, default=10): lv_size,
                cv.Optional(CONF_COLOR, default=0x808080): lv_color,
                cv.Optional(CONF_MAJOR): cv.Schema(
                    {
                        cv.Optional(CONF_STRIDE, default=3): cv.positive_int,
                        cv.Optional(CONF_WIDTH, default=5): lv_size,
                        cv.Optional(CONF_LENGTH, default="15%"): lv_size,
                        cv.Optional(CONF_COLOR, default=0): lv_color,
                        cv.Optional(CONF_LABEL_GAP, default=4): lv_size,
                    }
                ),
            }
        ),
        cv.Optional(CONF_RANGE_FROM, default=0.0): cv.float_,
        cv.Optional(CONF_RANGE_TO, default=100.0): cv.float_,
        cv.Optional(CONF_ANGLE_RANGE, default=270): cv.int_range(0, 360),
        cv.Optional(CONF_ROTATION): lv_angle,
        cv.Optional(CONF_INDICATORS): cv.ensure_list(INDICATOR_SCHEMA),
    }
)

METER_SCHEMA = {cv.Optional(CONF_SCALES): cv.ensure_list(SCALE_SCHEMA)}

STYLED_TEXT_SCHEMA = cv.maybe_simple_value(
    STYLE_SCHEMA.extend(TEXT_SCHEMA), key=CONF_TEXT
)
PAGE_SCHEMA = {
    cv.Optional(CONF_SKIP, default=False): lv_bool,
}

LABEL_SCHEMA = TEXT_SCHEMA.extend(
    {
        cv.Optional(CONF_RECOLOR): lv_bool,
        cv.Optional(CONF_LONG_MODE): lv_one_of(LV_LONG_MODES, prefix="LV_LABEL_LONG_"),
    }
)

CHECKBOX_SCHEMA = TEXT_SCHEMA

DROPDOWN_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SYMBOL): lv_text,
        cv.Optional(CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(CONF_DIR, default="BOTTOM"): lv_one_of(DIRECTIONS, "LV_DIR_"),
        cv.Optional(CONF_DROPDOWN_LIST): part_schema(CONF_DROPDOWN_LIST),
    }
)

DROPDOWN_SCHEMA = DROPDOWN_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(lv_option_string),
    }
)

DROPDOWN_MODIFY_SCHEMA = DROPDOWN_BASE_SCHEMA

ROLLER_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(CONF_MODE, default="NORMAL"): lv_one_of(
            ROLLER_MODES, "LV_ROLLER_MODE_"
        ),
    }
)

ROLLER_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(lv_option_string),
    }
)

ROLLER_MODIFY_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATED, default=True): lv_animated,
    }
)

LED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR): lv_color,
        cv.Optional(CONF_BRIGHTNESS): cv.templatable(cv.percentage),
    }
)

# For use by platform components
LVGL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
    }
)


def get_layout(layout=cv.UNDEFINED, flow="ROW_WRAP"):
    return cv.Schema(
        {
            cv.Optional(CONF_LAYOUT, default=layout): lv_one_of(
                ["FLEX", "GRID"], "LV_LAYOUT_"
            ),
            cv.Optional(CONF_FLEX_FLOW, default=flow): lv_one_of(
                FLEX_FLOWS, prefix="LV_FLEX_FLOW_"
            ),
        }
    )


ALIGN_TO_SCHEMA = {
    cv.Optional(CONF_ALIGN_TO): cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_obj_t),
            cv.Required(CONF_ALIGN): lv_one_of(ALIGN_ALIGNMENTS, prefix="LV_ALIGN_"),
            cv.Optional(CONF_X, default=0): pixels_or_percent,
            cv.Optional(CONF_Y, default=0): pixels_or_percent,
        }
    )
}


def grid_free_space(value):
    value = cv.Upper(value)
    if value.startswith("FR(") and value.endswith(")"):
        value = value.removesuffix(")").removeprefix("FR(")
        return f"LV_GRID_FR({cv.positive_int(value)})"
    raise cv.Invalid("must be a size in pixels, CONTENT or FR(nn)")


grid_spec = cv.Schema(
    [cv.Any(lv_size, lv_one_of(("CONTENT",), prefix="LV_GRID_"), grid_free_space)]
)

GRID_CONTAINER_SCHEMA = {
    cv.Optional(CONF_GRID_ROWS): grid_spec,
    cv.Optional(CONF_GRID_COLUMNS): grid_spec,
    cv.Optional(CONF_GRID_COLUMN_ALIGN): grid_alignments,
    cv.Optional(CONF_GRID_ROW_ALIGN): grid_alignments,
}


def obj_schema(wtype: str):
    return (
        part_schema(wtype)
        .extend(FLAG_SCHEMA)
        .extend(GRID_CONTAINER_SCHEMA)
        .extend(automation_schema(get_widget_type(wtype)))
        .extend(ALIGN_TO_SCHEMA)
        .extend(get_layout())
        .extend(
            cv.Schema(
                {
                    cv.Optional(CONF_STATE): SET_STATE_SCHEMA,
                    cv.Optional(CONF_GROUP): lv_id_name,
                }
            )
        )
    )


def container_schema(widget_type):
    lv_type = get_widget_type(widget_type)
    schema = obj_schema(widget_type)
    if extras := globals().get(f"{widget_type.upper()}_SCHEMA"):
        schema = schema.extend(extras).add_extra(validate_max_min)
    schema = schema.extend({cv.GenerateID(): cv.declare_id(lv_type)})

    # Delayed evaluation for recursion
    def validator(value):
        widgets = cv.Schema(
            {
                cv.Optional(CONF_WIDGETS): cv.ensure_list(WIDGET_SCHEMA),
            }
        )
        result = schema.extend(widgets)
        if value == SCHEMA_EXTRACT:
            return result
        return result(value)

    return validator


def widget_schema(name):
    validator = cv.All(container_schema(name), validate_grid)
    if required := REQUIRED_COMPONENTS.get(name):
        validator = cv.All(validator, requires_component(required))
    return cv.Exclusive(name, CONF_WIDGETS), validator


WIDGET_SCHEMA = cv.Any(dict(map(widget_schema, WIDGET_TYPES)))

MSGBOX_SCHEMA = STYLE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(lv_obj_t),
        cv.Required(CONF_TITLE): STYLED_TEXT_SCHEMA,
        cv.Optional(CONF_BODY): STYLED_TEXT_SCHEMA,
        cv.Optional(CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
        cv.Optional(CONF_CLOSE_BUTTON): lv_bool,
        cv.Optional(CONF_WIDGETS): cv.ensure_list(WIDGET_SCHEMA),
    }
)


async def get_color_value(value):
    if isinstance(value, Lambda):
        return f"{await cg.process_lambda(value, [], return_type=lv_color_t)}()"
    return value


async def get_end_value(config):
    return await lv_int.process(config.get(CONF_END_VALUE))


async def get_start_value(config):
    if CONF_START_VALUE in config:
        value = config[CONF_START_VALUE]
    else:
        value = config.get(CONF_VALUE)
    return await lv_int.process(value)


async def add_init_lambda(lv_component, init):
    lamb = await cg.process_lambda(
        Lambda(";\n".join([*init, ""])), [(lv_disp_t_ptr, "lv_disp")]
    )
    cg.add(lv_component.add_init_lambda(lamb))
    lv_temp_vars.clear()


def cgen(*args):
    cg.add(cg.RawExpression("\n".join(args)))


def styles_to_code(styles):
    """Convert styles to C__ code."""
    for style in styles:
        svar = cg.new_Pvariable(style[CONF_ID])
        cgen(f"lv_style_init({svar})")
        for prop in STYLE_PROPS:
            if prop in style:
                cgen(f"lv_style_set_{prop}({svar}, {style[prop]})")


theme_widget_map = {}
# Map of widgets to their config, used for trigger generation
widget_map = {}
widgets_completed = False  # will be set true when all widgets are available


def get_widget_generator(id):
    while True:
        if obj := widget_map.get(id):
            return obj
        if widgets_completed:
            raise cv.Invalid(
                f"Widget {id} not found, yet all widgets should be defined by now"
            )
        yield


async def get_widget(id: ID) -> Widget:
    if obj := widget_map.get(id):
        return obj
    return await FakeAwaitable(get_widget_generator(id))


async def theme_to_code(theme):
    for widget, style in theme.items():
        if not isinstance(style, dict):
            continue

        init = []
        ow = Widget("obj", get_widget_type(widget))
        init.extend(await set_obj_properties(ow, style))
        lamb = await cg.process_lambda(
            Lambda(";\n".join([*init, ""])),
            [(lv_obj_t_ptr, "obj")],
            capture="",
        )
        apply = f"lv_theme_apply_{widget}"
        theme_widget_map[widget] = apply
        lamb_id = ID(apply, type=lv_lambda_t, is_declaration=True)
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
    for prop in [*STYLE_PROPS, *OBJ_FLAGS, CONF_STYLES, CONF_GROUP]:
        if prop in config:
            props[prop] = config[prop]
    return props


def collect_states(config):
    states = {CONF_DEFAULT: collect_props(config)}
    for state in STATES:
        if state in config:
            states[state] = collect_props(config[state])
    return states


def collect_parts(config):
    parts = {CONF_MAIN: collect_states(config)}
    for part in PARTS:
        if part in config:
            parts[part] = collect_states(config[part])
    return parts


async def set_obj_properties(widget: Widget, config):
    """Return a list of C++ statements to apply properties to an lv_obj_t"""
    init = []
    parts = collect_parts(config)
    for part, states in parts.items():
        for state, props in states.items():
            lv_state = f"(int)LV_STATE_{state.upper()}|(int)LV_PART_{part.upper()}"
            if styles := props.get(CONF_STYLES):
                for style_id in styles:
                    init.append(
                        f"lv_obj_add_style({widget.obj}, {style_id}, {lv_state})"
                    )
            for prop, value in {
                k: v for k, v in props.items() if k in STYLE_PROPS
            }.items():
                if isinstance(STYLE_PROPS[prop], LValidator):
                    value = await STYLE_PROPS[prop].process(value)
                init.extend(widget.set_style(prop, value, lv_state))
    if group := add_group(config.get(CONF_GROUP)):
        init.append(f"lv_group_add_obj({group}, {widget.obj})")
    flag_clr = set()
    flag_set = set()
    props = parts[CONF_MAIN][CONF_DEFAULT]
    for prop, value in {k: v for k, v in props.items() if k in OBJ_FLAGS}.items():
        if value:
            flag_set.add(prop)
        else:
            flag_clr.add(prop)
    if flag_set:
        adds = join_enums(flag_set, "LV_OBJ_FLAG_")
        init.extend(widget.add_flag(adds))
    if flag_clr:
        clrs = join_enums(flag_clr, "LV_OBJ_FLAG_")
        init.extend(widget.clear_flag(clrs))

    if layout := config.get(CONF_LAYOUT):
        layout = layout.upper()
        init.extend(widget.set_property("layout", layout, "obj"))
        if layout == "LV_LAYOUT_FLEX":
            lv_uses.add("FLEX")
            init.extend(widget.set_property("flex_flow", config[CONF_FLEX_FLOW], "obj"))
        if layout == "LV_LAYOUT_GRID":
            lv_uses.add("GRID")
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
            adds = join_enums(adds, "LV_STATE_")
            init.extend(widget.add_state(adds))
        if clears:
            clears = join_enums(clears, "LV_STATE_")
            init.extend(widget.clear_state(clears))
        for key, value in lambs.items():
            lamb = await cg.process_lambda(value, [], return_type=cg.bool_)
            init.append(
                f"""
                if({lamb}())
                    lv_obj_add_state({widget.obj}, LV_STATE_{key.upper()});
                else
                    lv_obj_clear_state({widget.obj}, LV_STATE_{key.upper()});
                """
            )
    if scrollbar_mode := config.get(CONF_SCROLLBAR_MODE):
        init.append(f"lv_obj_set_scrollbar_mode({widget.obj}, {scrollbar_mode})")
    if config.get(CONF_LAYOUT) == "LV_LAYOUT_GRID":
        wid = config[CONF_ID]
        for key in (CONF_GRID_COLUMN_ALIGN, CONF_GRID_COLUMN_ALIGN):
            if value := config.get(key):
                init.extend(widget.set_property(key, value))
        rows = "{" + ",".join(config[CONF_GRID_ROWS]) + ", LV_GRID_TEMPLATE_LAST}"
        row_id = ID(f"{wid}_row_dsc", is_declaration=True, type=lv_coord_t)
        row_array = cg.static_const_array(row_id, cg.RawExpression(rows))
        init.extend(widget.set_style("grid_row_dsc_array", row_array, 0))
        columns = "{" + ",".join(config[CONF_GRID_COLUMNS]) + ", LV_GRID_TEMPLATE_LAST}"
        column_id = ID(f"{wid}_column_dsc", is_declaration=True, type=lv_coord_t)
        column_array = cg.static_const_array(column_id, cg.RawExpression(columns))
        init.extend(widget.set_style("grid_column_dsc_array", column_array, 0))
    return init


async def checkbox_to_code(var: Widget, checkbox_conf):
    """For a text object, create and set text"""
    if value := checkbox_conf.get(CONF_TEXT):
        return await lv_text.set_text(var, value)
    return []


async def label_to_code(var: Widget, label_conf):
    """For a text object, create and set text"""
    init = []
    if value := label_conf.get(CONF_TEXT):
        init.extend(await lv_text.set_text(var, value))
    if mode := label_conf.get(CONF_LONG_MODE):
        init.extend(var.set_property("long_mode", mode))
    if (recolor := label_conf.get(CONF_RECOLOR)) is not None:
        init.extend(var.set_property("recolor", recolor))
    return init


async def obj_to_code(var, obj):
    return []


async def page_to_code(config, pconf, index):
    """Write object creation code for an object extending lv_obj_t"""
    init = []
    var = cg.new_Pvariable(pconf[CONF_ID])
    page = Widget(var, lv_page_t, config, f"{var}->page")
    init.append(f"{var}->index = {index}")
    init.append(f"{page.obj} = lv_obj_create(nullptr)")
    skip = pconf[CONF_SKIP]
    init.append(f"{var}->skip = {skip}")
    # Set outer config first
    init.extend(await set_obj_properties(page, config))
    init.extend(await set_obj_properties(page, pconf))
    if CONF_WIDGETS in pconf:
        for widg in pconf[CONF_WIDGETS]:
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
    if color := config.get(CONF_COLOR):
        init.extend(var.set_property("color", color))
    if brightness := await lv_float.process(config.get(CONF_BRIGHTNESS)):
        init.extend(var.set_property("brightness", int(brightness * 255)))
    return init


SHOW_SCHEMA = LVGL_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATION, default="NONE"): lv_one_of(
            LV_ANIM, "LV_SCR_LOAD_ANIM_"
        ),
        cv.Optional(CONF_TIME, default="50ms"): cv.positive_time_period_milliseconds,
    }
)


@automation.register_action(
    "lvgl.animimg.start",
    ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_animimg_t),
        },
        key=CONF_ID,
    ),
)
async def animimg_start(config, action_id, template_arg, args):
    widget = await cg.get_variable(config[CONF_ID])
    init = [f"lv_animimg_start({widget})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.animimg.stop",
    ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_animimg_t),
        },
        key=CONF_ID,
    ),
)
async def animimg_stop(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = widget.set_property(CONF_DURATION, 0)
    init.append(f"lv_animimg_start({widget.obj})")
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.animimg.update",
    ObjUpdateAction,
    modify_schema(CONF_ANIMIMG),
)
async def animimg_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = await animimg_to_code(widget, config)
    init.append(f"lv_animimg_start({widget.obj})")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.page.next",
    ObjUpdateAction,
    SHOW_SCHEMA,
)
async def page_next_to_code(config, action_id, template_arg, args):
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    animation = config[CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp}->show_next_page(false, {animation}, {time})"]
    return await action_to_code(init, action_id, lv_comp, template_arg, args)


@automation.register_action(
    "lvgl.page.previous",
    ObjUpdateAction,
    SHOW_SCHEMA,
)
async def page_previous_to_code(config, action_id, template_arg, args):
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    animation = config[CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp}->show_next_page(true, {animation}, {time})"]
    return await action_to_code(init, action_id, lv_comp, template_arg, args)


@automation.register_action(
    "lvgl.page.show",
    ObjUpdateAction,
    cv.maybe_simple_value(
        SHOW_SCHEMA.extend(
            {
                cv.Required(CONF_ID): cv.use_id(lv_page_t),
            }
        ),
        key=CONF_ID,
    ),
)
async def page_show_to_code(config, action_id, template_arg, args):
    obj = await cg.get_variable(config[CONF_ID])
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    animation = config[CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp}->show_page({obj}->index, {animation}, {time})"]
    return await action_to_code(init, action_id, obj, template_arg, args)


@automation.register_action(
    "lvgl.led.update",
    ObjUpdateAction,
    modify_schema(CONF_LED),
)
async def led_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await led_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


async def roller_to_code(var, config):
    init = []
    mode = config[CONF_MODE]
    if options := config.get(CONF_OPTIONS):
        text = cg.safe_exp("\n".join(options))
        init.append(f"lv_roller_set_options({var.obj}, {text}, {mode})")
    animated = config.get(CONF_ANIMATED) or "LV_ANIM_OFF"
    if selected := config.get(CONF_SELECTED_INDEX):
        value = await lv_int.process(selected)
        init.append(f"lv_roller_set_selected({var.obj}, {value}, {animated})")
    return init


@automation.register_action(
    "lvgl.roller.update",
    ObjUpdateAction,
    modify_schema(CONF_ROLLER),
)
async def roller_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await roller_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


async def dropdown_to_code(dropdown: Widget, config):
    obj = dropdown.obj
    init = []
    if options := config.get(CONF_OPTIONS):
        text = cg.safe_exp("\n".join(options))
        init.extend(dropdown.set_property("options", text))
    if symbol := config.get(CONF_SYMBOL):
        init.extend(dropdown.set_property("symbol", await lv_text.process(symbol)))
    if selected := config.get(CONF_SELECTED_INDEX):
        value = await lv_int.process(selected)
        init.extend(dropdown.set_property("selected", value))
    if dir := config.get(CONF_DIR):
        init.extend(dropdown.set_property("dir", dir))
    if list := config.get(CONF_DROPDOWN_LIST):
        s = Widget(dropdown, lv_dropdown_list_t, list, f"{dropdown.obj}__list")
        init.extend(add_temp_var("lv_obj_t", s.obj))
        init.append(f"{s.obj} = lv_dropdown_get_list({obj});")
        init.extend(await set_obj_properties(s, list))
    return init


@automation.register_action(
    "lvgl.dropdown.update",
    ObjUpdateAction,
    modify_schema(CONF_DROPDOWN),
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
    for row in config:
        for btn in row[CONF_BUTTONS]:
            bid = btn[CONF_ID]
            widget = MatrixButton(btnm, lv_btnmatrix_t, btn, len(width_list))
            widget_map[bid] = widget
            if text := btn.get(CONF_TEXT):
                text_list.append(f"{cg.safe_exp(text)}")
            else:
                text_list.append("")
            key_list.append(btn.get(CONF_KEY_CODE) or 0)
            width_list.append(btn[CONF_WIDTH])
            ctrl = ["(int)LV_BTNMATRIX_CTRL_CLICK_TRIG"]
            if controls := btn.get(CONF_CONTROL):
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
    text_id = ID(f"{id.id}_text_array", is_declaration=True, type=char_ptr_const)
    text_id = cg.static_const_array(
        text_id, cg.RawExpression("{" + ",".join(text_list) + "}")
    )
    return text_id, ctrl_list, width_list, key_list


def set_btn_data(btnm: Widget, ctrl_list, width_list):
    init = []
    for index, ctrl in enumerate(ctrl_list):
        init.append(f"lv_btnmatrix_set_btn_ctrl({btnm.obj}, {index}, {ctrl})")
    for index, width in enumerate(width_list):
        init.append(f"lv_btnmatrix_set_btn_width({btnm.obj}, {index}, {width})")
    return init


async def btnmatrix_to_code(btnm: Widget, conf):
    id = conf[CONF_ID]
    text_id, ctrl_list, width_list, key_list = await get_button_data(
        conf[CONF_ROWS], id, btnm
    )
    init = [f"lv_btnmatrix_set_map({btnm.obj}, {text_id})"]
    init.extend(set_btn_data(btnm, ctrl_list, width_list))
    init.append(f"lv_btnmatrix_set_one_checked({btnm.obj}, {conf[CONF_ONE_CHECKED]})")
    for index, key in enumerate(key_list):
        if key != 0:
            init.append(f"{btnm.var}->set_key({index}, {key})")
    return init


async def msgbox_to_code(conf):
    """
    Construct a message box. This consists of a full-screen translucent background enclosing a centered container
    with an optional title, body, close button and a button matrix. And any other widgets the user cares to add
    :param conf: The config data
    :return: code to add to the init lambda
    """
    init = []
    id = conf[CONF_ID]
    outer = cg.new_variable(
        ID(id.id, is_declaration=True, type=lv_obj_t_ptr), cg.nullptr
    )
    btnm = cg.new_variable(
        ID(f"{id.id}_btnm", is_declaration=True, type=lv_obj_t_ptr), cg.nullptr
    )
    btnm_widg = Widget(btnm, lv_btnmatrix_t)
    widget_map[id] = btnm_widg
    text_id, ctrl_list, width_list, _ = await get_button_data((conf,), id, btnm_widg)
    msgbox = cg.new_variable(
        ID(f"{id.id}_msgbox", is_declaration=True, type=lv_obj_t_ptr), cg.nullptr
    )
    text = await lv_text.process(conf.get(CONF_BODY))
    title = await lv_text.process(conf.get(CONF_TITLE))
    close_button = conf[CONF_CLOSE_BUTTON]
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
        init.extend(set_btn_data(Widget(s, lv_obj_t), ctrl_list, width_list))
    return init


async def animimg_to_code(var: Widget, config):
    init = []
    wid = config[CONF_ID]
    if CONF_SRC in config:
        srcs = "{" + ",".join([f"lv_img_from({x.id})" for x in config[CONF_SRC]]) + "}"
        src_id = ID(f"{wid}_src", is_declaration=True, type=void_ptr)
        src_arry = cg.static_const_array(src_id, cg.RawExpression(srcs))
        count = len(config[CONF_SRC])
        init.append(f"lv_animimg_set_src({wid}, {src_arry}, {count})")
    repeat_count = config.get(CONF_REPEAT_COUNT)
    if repeat_count is not None:
        init.extend(var.set_property(CONF_REPEAT_COUNT, repeat_count))
    duration = config.get(CONF_DURATION)
    if duration is not None:
        init.extend(var.set_property(CONF_DURATION, duration.total_milliseconds))
    if config.get(CONF_AUTO_START):
        init.append(f"lv_animimg_start({var.obj})")
    return init


async def img_to_code(var: Widget, img):
    init = [f"lv_img_set_src({var.obj}, lv_img_from({img[CONF_SRC]}))"]
    if angle := img.get(CONF_ANGLE):
        pivot_x = img[CONF_PIVOT_X]
        pivot_y = img[CONF_PIVOT_Y]
        init.extend(
            [
                f"lv_img_set_pivot({var.obj}, {pivot_x}, {pivot_y})",
                f"lv_img_set_angle({var.obj}, {angle})",
            ]
        )
    if zoom := img.get(CONF_ZOOM):
        init.append(f"lv_img_set_zoom({var.obj}, {zoom})")
    if offset_x := img.get(CONF_OFFSET_X):
        init.append(f"lv_img_set_offset_x({var.obj}, {offset_x})")
    if offset_y := img.get(CONF_OFFSET_Y):
        init.append(f"lv_img_set_offset_y({var.obj}, {offset_y})")
    if antialias := img.get(CONF_ANTIALIAS):
        init.append(f"lv_img_set_antialias({var.obj}, {antialias})")
    if mode := img.get(CONF_MODE):
        init.append(f"lv_img_set_size_mode({var.obj}, {mode})")
    return init


async def line_to_code(var: Widget, line):
    """For a line object, create and add the points"""
    data = line[CONF_POINTS]
    point_list = data[CONF_POINTS]
    initialiser = cg.RawExpression(
        "{" + ",".join(map(lambda p: "{" + f"{p[0]}, {p[1]}" + "}", point_list)) + "}"
    )
    points = cg.static_const_array(data[CONF_ID], initialiser)
    return [f"lv_line_set_points({var.obj}, {points}, {len(point_list)})"]


async def meter_to_code(meter: Widget, meter_conf):
    """For a meter object, create and set parameters"""

    var = meter.obj
    init = []
    s = "meter_var"
    init.extend(add_temp_var("lv_meter_scale_t", s))
    for scale in meter_conf.get(CONF_SCALES) or ():
        rotation = 90 + (360 - scale[CONF_ANGLE_RANGE]) / 2
        if CONF_ROTATION in scale:
            rotation = scale[CONF_ROTATION] // 10
        init.append(f"{s} = lv_meter_add_scale({var})")
        init.append(
            f"lv_meter_set_scale_range({var}, {s}, {scale[CONF_RANGE_FROM]},"
            + f"{scale[CONF_RANGE_TO]}, {scale[CONF_ANGLE_RANGE]}, {rotation})",
        )
        if ticks := scale.get(CONF_TICKS):
            init.append(
                f"lv_meter_set_scale_ticks({var}, {s}, {ticks[CONF_COUNT]},"
                + f"{ticks[CONF_WIDTH]}, {ticks[CONF_LENGTH]}, {ticks[CONF_COLOR]})"
            )
            if CONF_MAJOR in ticks:
                major = ticks[CONF_MAJOR]
                init.append(
                    f"lv_meter_set_scale_major_ticks({var}, {s}, {major[CONF_STRIDE]},"
                    + f"{major[CONF_WIDTH]}, {major[CONF_LENGTH]}, {major[CONF_COLOR]},"
                    + f"{major[CONF_LABEL_GAP]})"
                )
        for indicator in scale.get(CONF_INDICATORS) or ():
            (t, v) = next(iter(indicator.items()))
            iid = v[CONF_ID]
            ivar = cg.new_variable(iid, cg.nullptr, type_=lv_meter_indicator_t_ptr)
            # Enable getting the meter to which this belongs.
            widget_map[iid] = Widget(var, get_widget_type(t), v, ivar)
            if t == CONF_LINE:
                init.append(
                    f"{ivar} = lv_meter_add_needle_line({var}, {s}, {v[CONF_WIDTH]},"
                    + f"{v[CONF_COLOR]}, {v[CONF_R_MOD]})"
                )
            if t == CONF_ARC:
                init.append(
                    f"{ivar} = lv_meter_add_arc({var}, {s}, {v[CONF_WIDTH]},"
                    + f"{v[CONF_COLOR]}, {v[CONF_R_MOD]})"
                )
            if t == CONF_TICKS:
                color_end = v[CONF_COLOR_START]
                if CONF_COLOR_END in v:
                    color_end = v[CONF_COLOR_END]
                init.append(
                    f"{ivar} = lv_meter_add_scale_lines({var}, {s}, {v[CONF_COLOR_START]},"
                    + f"{color_end}, {v[CONF_LOCAL]}, {v[CONF_R_MOD]})"
                )
            start_value = await get_start_value(v)
            end_value = await get_end_value(v)
            if start_value is not None:
                init.append(
                    f"lv_meter_set_indicator_start_value({var}, {ivar}, {start_value})"
                )
            if end_value is not None:
                init.append(
                    f"lv_meter_set_indicator_end_value({var}, {ivar}, {end_value})"
                )

    return init


async def spinner_to_code(spinner: Widget, config):
    return []


async def arc_to_code(arc: Widget, config):
    var = arc.obj
    init = [
        f"lv_arc_set_range({var}, {config[CONF_MIN_VALUE]}, {config[CONF_MAX_VALUE]})",
        f"lv_arc_set_bg_angles({var}, {config[CONF_START_ANGLE] // 10}, {config[CONF_END_ANGLE] // 10})",
        f"lv_arc_set_rotation({var}, {config[CONF_ROTATION] // 10})",
        f"lv_arc_set_mode({var}, {config[CONF_MODE]})",
        f"lv_arc_set_change_rate({var}, {config[CONF_CHANGE_RATE]})",
    ]
    if not config[CONF_ADJUSTABLE]:
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
    if CONF_ROTARY_ENCODERS not in config:
        return init
    lv_uses.add("ROTARY_ENCODER")
    for enc_conf in config[CONF_ROTARY_ENCODERS]:
        sensor = await cg.get_variable(enc_conf[CONF_SENSOR])
        lpt = enc_conf[CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = enc_conf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
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
    if CONF_TOUCHSCREENS not in config:
        return init
    lv_uses.add("TOUCHSCREEN")
    for touchconf in config[CONF_TOUCHSCREENS]:
        touchscreen = await cg.get_variable(touchconf[CONF_TOUCHSCREEN_ID])
        lpt = touchconf[CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = touchconf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
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
                if event in LV_EVENT_TRIGGERS
            }.items():
                event = LV_EVENT[event[3:].upper()]
                conf = conf[0]
                trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
                if isinstance(
                    widget.type, cg.MockObjClass
                ) and widget.type.inherits_from(lv_number_t):
                    args = [(cg.float_, "x")]
                    value = f"lv_{widget.type_base()}_get_value({obj})"
                else:
                    args = []
                    value = ""
                await automation.build_automation(trigger, args, conf)
                init.extend(widget.add_flag("LV_OBJ_FLAG_CLICKABLE"))
                init.extend(
                    widget.set_event_cb(
                        f"{trigger}->trigger({value});", f"LV_EVENT_{event.upper()}"
                    )
                )
            if on_value := widget.config.get(CONF_ON_VALUE):
                for conf in on_value:
                    trigger = cg.new_Pvariable(
                        conf[CONF_TRIGGER_ID],
                    )
                    await automation.build_automation(trigger, [(cg.float_, "x")], conf)
                    init.extend(
                        widget.set_event_cb(
                            f"{trigger}->trigger(lv_{widget.type_base()}_get_value({obj}))",
                            "LV_EVENT_VALUE_CHANGED",
                            f"{lv_component}->get_custom_change_event()",
                        )
                    )
            if align_to := widget.config.get(CONF_ALIGN_TO):
                target = widget_map[align_to[CONF_ID]].obj
                align = align_to[CONF_ALIGN]
                x = align_to[CONF_X]
                y = align_to[CONF_Y]
                init.append(f"lv_obj_align_to({obj}, {target}, {align}, {x}, {y})")

    return init


async def to_code(config):
    cg.add_library("lvgl/lvgl", "8.3.11")
    for comp in lvgl_components_required:
        add_define(f"LVGL_USES_{comp.upper()}")
    add_define("_STRINGIFY(x)", "_STRINGIFY_(x)")
    add_define("_STRINGIFY_(x)", "#x")
    add_define("LV_CONF_SKIP", "1")
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

    add_define("LV_LOG_LEVEL", f"LV_LOG_LEVEL_{config[CONF_LOG_LEVEL]}")
    for font in lv_fonts_used:
        add_define(f"LV_FONT_{font.upper()}")
    add_define("LV_COLOR_DEPTH", config[CONF_COLOR_DEPTH])
    default_font = config[CONF_DEFAULT_FONT]
    add_define("LV_FONT_DEFAULT", default_font)
    if is_esphome_font(default_font):
        add_define("LV_FONT_CUSTOM_DECLARE", f"LV_FONT_DECLARE(*{default_font})")

    if config[CONF_COLOR_DEPTH] == 16:
        add_define(
            "LV_COLOR_16_SWAP", "1" if config[CONF_BYTE_ORDER] == "big_endian" else "0"
        )
    add_define(
        "LV_COLOR_CHROMA_KEY", await lv_color.process(config[CONF_TRANSPARENCY_KEY])
    )
    CORE.add_build_flag("-Isrc")

    cg.add_global(lvgl_ns.using)
    lv_component = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(lv_component, config)
    displays = set()
    if display := config.get(CONF_DISPLAY_ID):
        displays.add(display)
    for display in config.get(CONF_DISPLAYS, []):
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
    cg.add(lv_component.set_full_refresh(config[CONF_FULL_REFRESH]))
    cgen("lv_init()")
    if CONF_ROTARY_ENCODERS in config:  # or CONF_KEYBOARDS in config
        cgen("lv_group_set_default(lv_group_create())")
    init = []
    for font in esphome_fonts_used:
        getter = cg.RawExpression(f"(new lvgl::FontEngine({font}))->get_lv_font()")
        cg.Pvariable(
            ID(f"{font}_as_lv_font_", True, lv_font_t.operator("const")), getter
        )
    if style_defs := config.get(CONF_STYLE_DEFINITIONS, []):
        styles_to_code(style_defs)
    if theme := config.get(CONF_THEME):
        await theme_to_code(theme)
    if msgboxes := config.get(CONF_MSGBOXES):
        for msgbox in msgboxes:
            init.extend(await msgbox_to_code(msgbox))
    lv_scr_act = Widget("lv_scr_act()", lv_obj_t, config, "lv_scr_act()")
    if top_conf := config.get(CONF_TOP_LAYER):
        top_layer = Widget("lv_disp_get_layer_top(lv_disp)", lv_obj_t)
        init.extend(await set_obj_properties(top_layer, top_conf))
        if widgets := top_conf.get(CONF_WIDGETS):
            for widg in widgets:
                w_type, w_cnfig = next(iter(widg.items()))
                ext_init = await widget_to_code(w_cnfig, w_type, top_layer)
                init.extend(ext_init)
    if widgets := config.get(CONF_WIDGETS):
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
    init.append(f"{lv_component}->set_page_wrap({config[CONF_PAGE_WRAP]})")
    init.extend(await generate_triggers(lv_component))
    init.extend(await touchscreens_to_code(lv_component, config))
    init.extend(await rotary_encoders_to_code(lv_component, config))
    if on_idle := config.get(CONF_ON_IDLE):
        for conf in on_idle:
            templ = await cg.templatable(conf[CONF_TIMEOUT], [], cg.uint32)
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], lv_component, templ)
            await automation.build_automation(trigger, [], conf)

    if bg_color := config.get(CONF_DISP_BG_COLOR):
        init.append(f"lv_disp_set_bg_color(lv_disp, {bg_color})")
    if bg_image := config.get(CONF_DISP_BG_IMAGE):
        init.append(f"lv_disp_set_bg_image(lv_disp, lv_img_from({bg_image}))")
        # add_define("LV_COLOR_SCREEN_TRANSP", "1")
    await add_init_lambda(lv_component, init)
    for use in lv_uses:
        CORE.add_build_flag(f"-DLV_USE_{use.upper()}=1")
    for macro, value in lv_defines.items():
        cg.add_build_flag(f"-D\\'{macro}\\'=\\'{value}\\'")


def indicator_update_schema(base):
    return base.extend({cv.Required(CONF_ID): cv.use_id(lv_meter_indicator_t)})


async def action_to_code(action, action_id, obj, template_arg, args):
    if isinstance(obj, MatrixButton):
        obj = obj.var.obj
    elif isinstance(obj, Widget):
        obj = obj.obj
    action.insert(0, f"if ({obj} == nullptr) return")
    lamb = await cg.process_lambda(Lambda(";\n".join([*action, ""])), args)
    var = cg.new_Pvariable(action_id, template_arg, lamb)
    return var


async def update_to_code(config, action_id, widget: Widget, init, template_arg, args):
    if config is not None:
        init.extend(await set_obj_properties(widget, config))
    return await action_to_code(init, action_id, widget.obj, template_arg, args)


CONFIG_SCHEMA = (
    cv.polling_component_schema("1s")
    .extend(obj_schema("obj"))
    .extend(
        {
            cv.Optional(CONF_ID, default=CONF_LVGL_COMPONENT): cv.declare_id(
                LvglComponent
            ),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(Display),
            cv.Optional(CONF_DISPLAYS): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_DISPLAY_ID): cv.use_id(Display),
                    },
                    key=CONF_DISPLAY_ID,
                ),
            ),
            cv.Optional(CONF_TOUCHSCREENS): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
                        cv.Optional(
                            CONF_LONG_PRESS_TIME, default="400ms"
                        ): cv.positive_time_period_milliseconds,
                        cv.Optional(
                            CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
                        ): cv.positive_time_period_milliseconds,
                        cv.GenerateID(): cv.declare_id(LVTouchListener),
                    },
                    key=CONF_TOUCHSCREEN_ID,
                )
            ),
            cv.Optional(CONF_ROTARY_ENCODERS): cv.All(
                cv.ensure_list(
                    cv.Schema(
                        {
                            cv.Required(CONF_SENSOR): cv.use_id(RotaryEncoderSensor),
                            cv.Optional(
                                CONF_LONG_PRESS_TIME, default="400ms"
                            ): cv.positive_time_period_milliseconds,
                            cv.Optional(
                                CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
                            ): cv.positive_time_period_milliseconds,
                            cv.Optional(CONF_BINARY_SENSOR): cv.use_id(BinarySensor),
                            cv.Optional(CONF_GROUP): lv_id_name,
                            cv.GenerateID(): cv.declare_id(LVRotaryEncoderListener),
                        }
                    )
                ),
            ),
            cv.Optional(CONF_COLOR_DEPTH, default=16): cv.one_of(1, 8, 16, 32),
            cv.Optional(CONF_DEFAULT_FONT, default="montserrat_14"): lv_font,
            cv.Optional(CONF_FULL_REFRESH, default=False): cv.boolean,
            cv.Optional(CONF_BUFFER_SIZE, default="100%"): cv.percentage,
            cv.Optional(CONF_LOG_LEVEL, default="WARN"): cv.one_of(
                *LOG_LEVELS, upper=True
            ),
            cv.Optional(CONF_BYTE_ORDER, default="big_endian"): cv.one_of(
                "big_endian", "little_endian"
            ),
            cv.Optional(CONF_STYLE_DEFINITIONS): cv.ensure_list(
                cv.Schema({cv.Required(CONF_ID): cv.declare_id(lv_style_t)}).extend(
                    STYLE_SCHEMA
                )
            ),
            cv.Optional(CONF_ON_IDLE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IdleTrigger),
                    cv.Required(CONF_TIMEOUT): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                }
            ),
            cv.Exclusive(CONF_WIDGETS, CONF_PAGES): cv.ensure_list(WIDGET_SCHEMA),
            cv.Exclusive(CONF_PAGES, CONF_PAGES): cv.ensure_list(
                container_schema(CONF_PAGE)
            ),
            cv.Optional(CONF_MSGBOXES): cv.ensure_list(MSGBOX_SCHEMA),
            cv.Optional(CONF_PAGE_WRAP, default=True): lv_bool,
            cv.Optional(CONF_TOP_LAYER): container_schema(CONF_OBJ),
            cv.Optional(CONF_TRANSPARENCY_KEY, default=0x000400): lv_color,
            cv.Optional(CONF_THEME): cv.Schema(
                {cv.Optional(w): obj_schema(w) for w in WIDGET_TYPES}
            ),
            cv.Exclusive(CONF_DISP_BG_IMAGE, CONF_DISP_BG_COLOR): cv.use_id(Image_),
            cv.Exclusive(CONF_DISP_BG_COLOR, CONF_DISP_BG_COLOR): lv_color,
        }
    )
).add_extra(
    cv.All(
        cv.has_at_least_one_key(CONF_PAGES, CONF_WIDGETS),
    )
)


def spinner_obj_creator(parent: Widget, config: dict):
    return f"lv_spinner_create({parent.obj}, {config[CONF_SPIN_TIME].total_milliseconds}, {config[CONF_ARC_LENGTH] // 10})"


async def widget_to_code(w_cnfig, w_type, parent: Widget):
    init = []

    creator = f"{w_type}_obj_creator"
    if creator := globals().get(creator):
        creator = creator(parent, w_cnfig)
    else:
        creator = f"lv_{w_type}_create({parent.obj})"
    lv_uses.add(w_type)
    id = w_cnfig[CONF_ID]
    if id.type.inherits_from(LvCompound):
        var = cg.new_Pvariable(id)
        init.append(f"{var}->set_obj({creator})")
        obj = f"{var}->obj"
    else:
        var = cg.Pvariable(w_cnfig[CONF_ID], cg.nullptr, type_=lv_obj_t)
        init.append(f"{var} = {creator}")
        obj = var

    widget = Widget(var, get_widget_type(w_type), w_cnfig, obj)
    widget_map[id] = widget
    if theme := theme_widget_map.get(w_type):
        init.append(f"{theme}({obj})")
    init.extend(await set_obj_properties(widget, w_cnfig))
    if widgets := w_cnfig.get(CONF_WIDGETS):
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
        cv.Required(CONF_ID): cv.use_id(lv_pseudo_button_t),
    },
    key=CONF_ID,
)


@automation.register_action("lvgl.widget.disable", ObjUpdateAction, ACTION_SCHEMA)
async def obj_disable_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.add_state("LV_STATE_DISABLED")
    return await action_to_code(action, action_id, widget.obj, template_arg, args)


@automation.register_action("lvgl.widget.enable", ObjUpdateAction, ACTION_SCHEMA)
async def obj_enable_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.clear_state("LV_STATE_DISABLED")
    return await action_to_code(action, action_id, widget.obj, template_arg, args)


@automation.register_action("lvgl.widget.show", ObjUpdateAction, ACTION_SCHEMA)
async def obj_show_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.clear_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(action, action_id, widget.obj, template_arg, args)


@automation.register_action("lvgl.widget.hide", ObjUpdateAction, ACTION_SCHEMA)
async def obj_hide_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    widget = await get_widget(obj_id)
    action = widget.add_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(action, action_id, widget.obj, template_arg, args)


@automation.register_action(
    "lvgl.widget.update", ObjUpdateAction, modify_schema(CONF_OBJ)
)
async def obj_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    return await update_to_code(config, action_id, obj, [], template_arg, args)


@automation.register_action(
    "lvgl.checkbox.update",
    ObjUpdateAction,
    modify_schema(CONF_CHECKBOX),
)
async def checkbox_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await checkbox_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


@automation.register_action(
    "lvgl.label.update",
    ObjUpdateAction,
    modify_schema(CONF_LABEL),
)
async def label_update_to_code(config, action_id, template_arg, args):
    obj = await get_widget(config[CONF_ID])
    init = await label_to_code(obj, config)
    return await update_to_code(config, action_id, obj, init, template_arg, args)


@automation.register_action(
    "lvgl.indicator.line.update",
    ObjUpdateAction,
    indicator_update_schema(INDICATOR_LINE_SCHEMA),
)
async def indicator_update_to_code(config, action_id, template_arg, args):
    ind = await get_widget(config[CONF_ID])
    init = []
    start_value = await get_start_value(config)
    end_value = await get_end_value(config)
    selector = "start_" if end_value is not None else ""
    if start_value is not None:
        init.append(
            f"lv_meter_set_indicator_{selector}value({ind.var},{ind.obj}, {start_value})"
        )
    if end_value is not None:
        init.append(
            f"lv_meter_set_indicator_end_value({ind.var},{ind.obj}, {end_value})"
        )
    return await update_to_code(None, action_id, ind, init, template_arg, args)


@automation.register_action(
    "lvgl.button.update",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Optional(CONF_WIDTH): cv.positive_int,
            cv.Optional(CONF_CONTROL): cv.ensure_list(
                cv.Schema(
                    {cv.Optional(k.lower()): cv.boolean for k in BTNMATRIX_CTRLS}
                ),
            ),
            cv.Required(CONF_ID): cv.use_id(LvBtnmBtn),
            cv.Optional(CONF_SELECTED): lv_bool,
        }
    ),
)
async def button_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    assert isinstance(widget, MatrixButton)
    init = []
    if (width := config.get(CONF_WIDTH)) is not None:
        init.extend(widget.set_width(width))
    if config.get(CONF_SELECTED):
        init.extend(widget.set_selected())
    if controls := config.get(CONF_CONTROL):
        adds = []
        clrs = []
        for item in controls:
            adds.extend([k for k, v in item.items() if v])
            clrs.extend([k for k, v in item.items() if not v])
        if adds:
            init.extend(widget.set_ctrls(*adds))
        if clrs:
            init.extend(widget.clear_ctrls(*clrs))
    return await action_to_code(init, action_id, widget.var.obj, template_arg, args)


@automation.register_action(
    "lvgl.spinner.update",
    ObjUpdateAction,
    modify_schema(CONF_SPINNER),
)
async def spinner_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.btnmatrix.update",
    ObjUpdateAction,
    modify_schema(CONF_BTNMATRIX),
)
async def btnmatrix_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.arc.update",
    ObjUpdateAction,
    modify_schema(CONF_ARC),
)
async def arc_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    value = await lv_int.process(config.get(CONF_VALUE))
    init.append(f"lv_arc_set_value({widget.obj}, {value})")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


async def slider_to_code(slider: Widget, config):
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
    ObjUpdateAction,
    modify_schema(CONF_SLIDER),
)
async def slider_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    animated = config[CONF_ANIMATED]
    value = await lv_int.process(config.get(CONF_VALUE))
    init.append(f"lv_slider_set_value({widget.obj}, {value}, {animated})")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.img.update",
    ObjUpdateAction,
    modify_schema(CONF_IMG),
)
async def img_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = []
    if src := config.get(CONF_SRC):
        init.append(f"lv_img_set_src({widget.obj}, lv_img_from({src}))")
    return await update_to_code(config, action_id, widget, init, template_arg, args)


@automation.register_action(
    "lvgl.widget.redraw",
    LvglAction,
    cv.Schema(
        {
            cv.Optional(CONF_ID): cv.use_id(lv_obj_t),
            cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
        }
    ),
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_LVGL_ID])
    if obj_id := config.get(CONF_ID):
        obj = cg.get_variable(obj_id)
    else:
        obj = "lv_scr_act()"
    lamb = await cg.process_lambda(
        Lambda(f"lv_obj_invalidate({obj});"), [(LvglComponentPtr, "lvgl_comp")]
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.pause",
    LvglAction,
    {
        cv.GenerateID(): cv.use_id(LvglComponent),
        cv.Optional(CONF_SHOW_SNOW, default="false"): lv_bool,
    },
)
async def pause_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    lamb = await cg.process_lambda(
        Lambda(f"lvgl_comp->set_paused(true, {config[CONF_SHOW_SNOW]});"),
        [(LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.resume",
    LvglAction,
    {
        cv.GenerateID(): cv.use_id(LvglComponent),
    },
)
async def resume_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    lamb = await cg.process_lambda(
        Lambda("lvgl_comp->set_paused(false, false);"),
        [(LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_condition(
    "lvgl.is_idle",
    LvglCondition,
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
    lvgl = config[CONF_LVGL_ID]
    timeout = await cg.templatable(config[CONF_TIMEOUT], [], cg.uint32)
    if isinstance(timeout, LambdaExpression):
        timeout = f"({timeout}())"
    else:
        timeout = timeout.total_milliseconds
    await cg.register_parented(var, lvgl)
    lamb = await cg.process_lambda(
        Lambda(f"return lvgl_comp->is_idle({timeout});"),
        [(LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_condition_lambda(lamb))
    return var


@automation.register_condition(
    "lvgl.is_paused",
    LvglCondition,
    LVGL_SCHEMA,
)
async def lvgl_is_paused(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    lvgl = config[CONF_LVGL_ID]
    await cg.register_parented(var, lvgl)
    lamb = await cg.process_lambda(
        Lambda("return lvgl_comp->is_paused();"), [(LvglComponentPtr, "lvgl_comp")]
    )
    cg.add(var.set_condition_lambda(lamb))
    return var
