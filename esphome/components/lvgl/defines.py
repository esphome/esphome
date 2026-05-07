"""
This is the base of the import tree for LVGL. It contains constant definitions used elsewhere.
Constants already defined in esphome.const are not duplicated here and must be imported where used.

"""

import logging
from typing import Any

from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_ITEMS
from esphome.core import CORE, ID, Lambda
from esphome.cpp_generator import (
    CallExpression,
    LambdaExpression,
    MockObj,
    MockObjClass,
)
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import Expression, SafeExpType

from .helpers import requires_component

LOGGER = logging.getLogger(__name__)
lvgl_ns = cg.esphome_ns.namespace("lvgl")

DOMAIN = "lvgl"
KEY_COLOR_FORMATS = "color_formats"
KEY_LV_DEFINES = "lv_defines"
KEY_REMAPPED_USES = "remapped_uses"
KEY_UPDATED_WIDGETS = "updated_widgets"
KEY_OPTIONS = "options"
KEY_WARNINGS = "warnings"


def get_data(key, default=None):
    """
    Get a data structure from the global data store by key
    :param key: A key for the data
    :param default: The default data - the default is an empty dict
    :return:
    """
    return CORE.data.setdefault(DOMAIN, {}).setdefault(
        key, {} if default is None else default
    )


def get_warnings():
    return get_data(KEY_WARNINGS, set())


def get_remapped_uses():
    return get_data(KEY_REMAPPED_USES, set())


def add_warning(msg: str):
    get_warnings().add(msg)


def get_options():
    return get_data(KEY_OPTIONS)


class StaticCastExpression(Expression):
    __slots__ = ("type", "exp")

    def __init__(self, type: Any, exp: SafeExpType):
        self.type = str(type)
        self.exp = cg.safe_exp(exp)

    def __str__(self):
        return f"static_cast<{self.type}>({self.exp})"


def add_define(macro, value="1"):
    lv_defines = get_data(KEY_LV_DEFINES)
    value = str(value)
    if lv_defines.setdefault(macro, value) != value:
        LOGGER.error(
            "Redefinition of %s - was %s now %s", macro, lv_defines[macro], value
        )
    lv_defines[macro] = value


def is_defined(macro):
    return macro in get_data(KEY_LV_DEFINES)


def literal(arg) -> MockObj:
    if isinstance(arg, str):
        return MockObj(arg)
    return arg


def addr(arg) -> MockObj:
    return MockObj(f"&{arg}")


def call_lambda(lamb: LambdaExpression):
    """
    Given a lambda, either reduce to a simple expression or call it, possibly with parameters
    from the surrounding context
    :param lamb:
    :return:
    """
    expr = lamb.content.strip()
    if expr.startswith("return") and expr.endswith(";"):
        # Convert a lambda returning a simple expression to just that expression
        expr = cg.RawExpression(expr[6:-1].strip())
        # Don't cast if the return type is a class
        if isinstance(lamb.return_type, MockObjClass):
            return expr
        return StaticCastExpression(lamb.return_type, expr)
    # If lambda has parameters, call it with their names
    # Parameter names come from hardcoded component code (like "x", "it", "event")
    # not from user input, so they're safe to use directly
    if lamb.parameters and lamb.parameters.parameters:
        return CallExpression(
            lamb, *[MockObj(x.id) for x in lamb.parameters.parameters]
        )
    return CallExpression(lamb)


class LValidator:
    """
    A validator for a particular type used in LVGL. Usable in configs as a validator, also
    has `process()` to convert a value during code generation
    """

    def __init__(self, validator, rtype: MockObj, retmapper=None, requires=None):
        self.validator = validator
        self.rtype = rtype
        self.retmapper = retmapper
        self.requires = requires

    def __call__(self, value):
        if self.requires:
            value = requires_component(self.requires)(value)
        if isinstance(value, cv.Lambda):
            return cv.returning_lambda(value)
        return self.validator(value)

    async def process(
        self, value: Any, args: list[tuple[SafeExpType, str]] | None = None
    ) -> Expression:
        if value is None:
            return None
        if isinstance(value, Lambda):
            # Local import to avoid circular import
            from .lvcode import get_lambda_context_args

            args = args or get_lambda_context_args()

            return call_lambda(
                await cg.process_lambda(value, args, return_type=self.rtype)
            )
        if self.retmapper is not None:
            return self.retmapper(value)
        if isinstance(value, ID):
            return await cg.get_variable(value)
        if isinstance(value, list):
            value = [
                await cg.get_variable(x) if isinstance(x, ID) else x for x in value
            ]
        if self.rtype is cg.int_:
            value = int(value)
        return cg.safe_exp(value)


class LvConstant(LValidator):
    """
    Allow one of a list of choices, mapped to upper case, and prepend the choice with the prefix.
    It's also permitted to include the prefix in the value
    The property `one_of` has the single case validator, and `several_of` allows a list of constants.
    """

    def __init__(self, prefix: str, *choices, typename=None):
        self.prefix = prefix
        self.choices = tuple(x.upper() for x in choices)
        self.typename = typename or prefix.lower() + "t"
        prefixed_choices = [prefix + v.upper() for v in choices]
        prefixed_validator = cv.one_of(*prefixed_choices, upper=True)

        @schema_extractor("one_of")
        def validator(value):
            if value == SCHEMA_EXTRACT:
                return self.choices
            if isinstance(value, str) and value.startswith(self.prefix):
                return prefixed_validator(value)
            return self.prefix + cv.one_of(*choices, upper=True)(value)

        super().__init__(validator, rtype=cg.uint32)
        self.retmapper = self.mapper
        self.one_of = LValidator(validator, cg.uint32, retmapper=self.mapper)
        self.several_of = LValidator(
            cv.ensure_list(self.one_of), cg.uint32, retmapper=self.mapper
        )

    def mapper(self, value):
        if not isinstance(value, list):
            value = [value]
        value = [
            (
                str(v).upper()
                if str(v).startswith(self.prefix)
                else self.prefix + str(v).upper()
            )
            for v in value
        ]
        if len(value) == 1:
            return literal(value[0])
        value = literal("|".join(value))
        if self.typename is None:
            return value
        return StaticCastExpression(self.typename, value)

    def extend(self, *choices):
        """
        Extend an LVconstant with additional choices.
        :param choices: The extra choices
        :return: A new LVConstant instance
        """
        return LvConstant(
            self.prefix, *(self.choices + choices), typename=self.typename
        )

    def __getattr__(self, item):
        if item.upper() not in self.choices:
            raise AttributeError(f"{item} not one of {self.choices}")
        return self.mapper(item)


# Parts
CONF_MAIN = "main"
CONF_SCROLLBAR = "scrollbar"
CONF_INDICATOR = "indicator"
CONF_KNOB = "knob"
CONF_SELECTED = "selected"
CONF_TICKS = "ticks"
CONF_CURSOR = "cursor"
CONF_TEXTAREA_PLACEHOLDER = "textarea_placeholder"

# Layout types

TYPE_FLEX = "flex"
TYPE_GRID = "grid"
TYPE_NONE = "none"

DIRECTIONS = LvConstant("LV_DIR_", "LEFT", "RIGHT", "BOTTOM", "TOP")

LV_FONTS = list(f"montserrat_{s}" for s in range(8, 50, 2)) + [
    "dejavu_16_persian_hebrew",
    "simsun_16_cjk",
    "unscii_8",
    "unscii_16",
]

LV_EVENT_MAP = {
    "ALL_EVENTS": "ALL",
    "CANCEL": "CANCEL",
    "CHANGE": "VALUE_CHANGED",
    "CHILD_CHANGE": "CHILD_CHANGED",
    "CHILD_CREATE": "CHILD_CREATED",
    "CHILD_DELETE": "CHILD_DELETED",
    "CLICK": "CLICKED",
    "COLOR_FORMAT_CHANGE": "COLOR_FORMAT_CHANGED",
    "COVER_CHECK": "COVER_CHECK",
    "CREATE": "CREATE",
    "DEFOCUS": "DEFOCUSED",
    "DELETE": "DELETE",
    "DOUBLE_CLICK": "DOUBLE_CLICKED",
    "DRAW_MAIN": "DRAW_MAIN",
    "DRAW_MAIN_BEGIN": "DRAW_MAIN_BEGIN",
    "DRAW_MAIN_END": "DRAW_MAIN_END",
    "DRAW_POST": "DRAW_POST",
    "DRAW_POST_BEGIN": "DRAW_POST_BEGIN",
    "DRAW_POST_END": "DRAW_POST_END",
    "DRAW_TASK_ADD": "DRAW_TASK_ADDED",
    "FOCUS": "FOCUSED",
    "GESTURE": "GESTURE",
    "GET_SELF_SIZE": "GET_SELF_SIZE",
    "HIT_TEST": "HIT_TEST",
    "HOVER_LEAVE": "HOVER_LEAVE",
    "HOVER_OVER": "HOVER_OVER",
    "INDEV_RESET": "INDEV_RESET",
    "INSERT": "INSERT",
    "INVALIDATE_AREA": "INVALIDATE_AREA",
    "KEY": "KEY",
    "LAYOUT_CHANGE": "LAYOUT_CHANGED",
    "LEAVE": "LEAVE",
    "LONG_PRESS": "LONG_PRESSED",
    "LONG_PRESS_REPEAT": "LONG_PRESSED_REPEAT",
    "PRESS": "PRESSED",
    "PRESS_LOST": "PRESS_LOST",
    "PRESSING": "PRESSING",
    "READY": "READY",
    "REFRESH": "REFRESH",
    "REFR_EXT_DRAW_SIZE": "REFR_EXT_DRAW_SIZE",
    "RELEASE": "RELEASED",
    "ROTARY": "ROTARY",
    "SCROLL": "SCROLL",
    "SCROLL_BEGIN": "SCROLL_BEGIN",
    "SCROLL_END": "SCROLL_END",
    "SCROLL_THROW_BEGIN": "SCROLL_THROW_BEGIN",
    "SHORT_CLICK": "SHORT_CLICKED",
    "SINGLE_CLICK": "SINGLE_CLICKED",
    "SIZE_CHANGE": "SIZE_CHANGED",
    "STATE_CHANGE": "STATE_CHANGED",
    "STYLE_CHANGE": "STYLE_CHANGED",
    "TRIPLE_CLICK": "TRIPLE_CLICKED",
}

LV_PRESS_EVENTS = ("PRESS", "PRESSING", "RELEASE")


def is_press_event(event: str) -> bool:
    return event.removeprefix("on_").upper() in LV_PRESS_EVENTS


LV_SCREEN_EVENT_MAP = {
    "SCREEN_LOAD": "SCREEN_LOADED",
    "SCREEN_LOAD_START": "SCREEN_LOAD_START",
    "SCREEN_UNLOAD": "SCREEN_UNLOADED",
    "SCREEN_UNLOAD_START": "SCREEN_UNLOAD_START",
}

LV_DISPLAY_EVENT_MAP = {
    "FLUSH_FINISH": "FLUSH_FINISH",
    "FLUSH_START": "FLUSH_START",
    "FLUSH_WAIT_FINISH": "FLUSH_WAIT_FINISH",
    "FLUSH_WAIT_START": "FLUSH_WAIT_START",
    "REFR_READY": "REFR_READY",
    "REFR_REQUEST": "REFR_REQUEST",
    "REFR_START": "REFR_START",
    "RENDER_READY": "RENDER_READY",
    "RENDER_START": "RENDER_START",
    "RESOLUTION_CHANGE": "RESOLUTION_CHANGED",
    "UPDATE_LAYOUT_COMPLETE": "UPDATE_LAYOUT_COMPLETED",
    "VSYNC": "VSYNC",
    "VSYNC_REQUEST": "VSYNC_REQUEST",
}

LV_EVENT_TRIGGERS = tuple(f"on_{x.lower()}" for x in LV_EVENT_MAP)
LV_DISPLAY_EVENT_TRIGGERS = tuple(f"on_{x.lower()}" for x in LV_DISPLAY_EVENT_MAP)
LV_SCREEN_EVENT_TRIGGERS = tuple(f"on_{x.lower()}" for x in LV_SCREEN_EVENT_MAP)

SWIPE_TRIGGERS = tuple(
    f"on_swipe_{x.lower()}" for x in DIRECTIONS.choices + ("up", "down")
)


LV_ANIM = LvConstant(
    "LV_SCREEN_LOAD_ANIM_",
    "NONE",
    "OVER_LEFT",
    "OVER_RIGHT",
    "OVER_TOP",
    "OVER_BOTTOM",
    "MOVE_LEFT",
    "MOVE_RIGHT",
    "MOVE_TOP",
    "MOVE_BOTTOM",
    "FADE_IN",
    "FADE_OUT",
    "OUT_LEFT",
    "OUT_RIGHT",
    "OUT_TOP",
    "OUT_BOTTOM",
)

LV_GRAD_DIR = LvConstant("LV_GRAD_DIR_", "NONE", "HOR", "VER")
LV_DITHER = LvConstant("LV_DITHER_", "NONE", "ORDERED", "ERR_DIFF")

LV_LOG_LEVELS = {
    "VERBOSE": "TRACE",
    "DEBUG": "TRACE",
    "INFO": "INFO",
    "WARN": "WARN",
    "ERROR": "ERROR",
    "NONE": "NONE",
}

LV_LONG_MODES = LvConstant(
    "LV_LABEL_LONG_",
    "WRAP",
    "DOT",
    "SCROLL",
    "SCROLL_CIRCULAR",
    "CLIP",
)

STATES = (
    # default state not included here
    "checked",
    "focused",
    "focus_key",
    "edited",
    "hovered",
    "pressed",
    "scrolled",
    "disabled",
    "user_1",
    "user_2",
    "user_3",
    "user_4",
)

PARTS = (
    CONF_MAIN,
    CONF_SCROLLBAR,
    CONF_INDICATOR,
    CONF_KNOB,
    CONF_SELECTED,
    CONF_ITEMS,
    # CONF_TICKS,
    CONF_CURSOR,
    CONF_TEXTAREA_PLACEHOLDER,
)

LV_PART = LvConstant("LV_PART_", *(p.upper() for p in PARTS))

KEYBOARD_MODES = LvConstant(
    "LV_KEYBOARD_MODE_",
    "TEXT_LOWER",
    "TEXT_UPPER",
    "SPECIAL",
    "NUMBER",
)
ROLLER_MODES = LvConstant("LV_ROLLER_MODE_", "NORMAL", "INFINITE")
TILE_DIRECTIONS = DIRECTIONS.extend("HOR", "VER", "ALL")
SCROLL_DIRECTIONS = TILE_DIRECTIONS.extend("NONE")
SNAP_DIRECTIONS = LvConstant("LV_SCROLL_SNAP_", "NONE", "START", "END", "CENTER")
CHILD_ALIGNMENTS = LvConstant(
    "LV_ALIGN_",
    "TOP_LEFT",
    "TOP_MID",
    "TOP_RIGHT",
    "LEFT_MID",
    "CENTER",
    "RIGHT_MID",
    "BOTTOM_LEFT",
    "BOTTOM_MID",
    "BOTTOM_RIGHT",
)

SIBLING_ALIGNMENTS = LvConstant(
    "LV_ALIGN_",
    "OUT_LEFT_TOP",
    "OUT_TOP_LEFT",
    "OUT_TOP_MID",
    "OUT_TOP_RIGHT",
    "OUT_RIGHT_TOP",
    "OUT_LEFT_MID",
    "OUT_RIGHT_MID",
    "OUT_LEFT_BOTTOM",
    "OUT_BOTTOM_LEFT",
    "OUT_BOTTOM_MID",
    "OUT_BOTTOM_RIGHT",
    "OUT_RIGHT_BOTTOM",
)
ALIGN_ALIGNMENTS = CHILD_ALIGNMENTS.extend(*SIBLING_ALIGNMENTS.choices)

FLEX_FLOWS = LvConstant(
    "LV_FLEX_FLOW_",
    "ROW",
    "COLUMN",
    "ROW_WRAP",
    "COLUMN_WRAP",
    "ROW_REVERSE",
    "COLUMN_REVERSE",
    "ROW_WRAP_REVERSE",
    "COLUMN_WRAP_REVERSE",
)

OBJ_FLAGS = (
    "hidden",
    "clickable",
    "click_focusable",
    "checkable",
    "scrollable",
    "scroll_elastic",
    "scroll_momentum",
    "scroll_one",
    "scroll_chain_hor",
    "scroll_chain_ver",
    "scroll_chain",
    "scroll_on_focus",
    "scroll_with_arrow",
    "snappable",
    "press_lock",
    "event_bubble",
    "gesture_bubble",
    "adv_hittest",
    "ignore_layout",
    "floating",
    "overflow_visible",
    "layout_1",
    "layout_2",
    "send_draw_task_events",
    "widget_1",
    "widget_2",
    "user_1",
    "user_2",
    "user_3",
    "user_4",
)
LV_OBJ_FLAG = LvConstant("LV_OBJ_FLAG_", *OBJ_FLAGS)

ARC_MODES = LvConstant("LV_ARC_MODE_", "NORMAL", "REVERSE", "SYMMETRICAL")
BAR_MODES = LvConstant("LV_BAR_MODE_", "NORMAL", "SYMMETRICAL", "RANGE")
SLIDER_MODES = LvConstant("LV_SLIDER_MODE_", "NORMAL", "SYMMETRICAL", "RANGE")

BUTTONMATRIX_CTRLS = LvConstant(
    "LV_BUTTONMATRIX_CTRL_",
    "HIDDEN",
    "NO_REPEAT",
    "DISABLED",
    "CHECKABLE",
    "CHECKED",
    "CLICK_TRIG",
    "POPOVER",
    "RECOLOR",
    "CUSTOM_1",
    "CUSTOM_2",
)

LV_BASE_ALIGNMENTS = (
    "START",
    "CENTER",
    "END",
)
LV_CELL_ALIGNMENTS = LvConstant(
    "LV_GRID_ALIGN_",
    *LV_BASE_ALIGNMENTS,
)
LV_GRID_ALIGNMENTS = LV_CELL_ALIGNMENTS.extend(
    "STRETCH",
    "SPACE_EVENLY",
    "SPACE_AROUND",
    "SPACE_BETWEEN",
)

LV_FLEX_ALIGNMENTS = LvConstant(
    "LV_FLEX_ALIGN_",
    *LV_BASE_ALIGNMENTS,
    "SPACE_EVENLY",
    "SPACE_AROUND",
    "SPACE_BETWEEN",
)

LV_FLEX_CROSS_ALIGNMENTS = LV_FLEX_ALIGNMENTS.extend("STRETCH")

LV_MENU_MODES = LvConstant(
    "LV_MENU_HEADER_",
    "TOP_FIXED",
    "TOP_UNFIXED",
    "BOTTOM_FIXED",
)

LV_CHART_TYPES = (
    "NONE",
    "LINE",
    "BAR",
    "SCATTER",
)
LV_CHART_AXES = (
    "PRIMARY_Y",
    "SECONDARY_Y",
    "PRIMARY_X",
    "SECONDARY_X",
)

CONF_ACCEPTED_CHARS = "accepted_chars"
CONF_ADJUSTABLE = "adjustable"
CONF_ALIGN = "align"
CONF_ALIGN_TO = "align_to"
CONF_ALIGN_TO_LAMBDA_ID = "align_to_lambda_id"
CONF_ANGLE_RANGE = "angle_range"
CONF_ANIMATED = "animated"
CONF_ANIMATION = "animation"
CONF_ANIMATIONS = "animations"
CONF_ANTIALIAS = "antialias"
CONF_ARC_LENGTH = "arc_length"
CONF_AUTO_START = "auto_start"
CONF_BACKGROUND_STYLE = "background_style"
CONF_BG_OPA = "bg_opa"
CONF_BOTTOM_LAYER = "bottom_layer"
CONF_BUTTON_STYLE = "button_style"
CONF_DECIMAL_PLACES = "decimal_places"
CONF_COLUMN = "column"
CONF_DIGITS = "digits"
CONF_DISP_BG_COLOR = "disp_bg_color"
CONF_DISP_BG_IMAGE = "disp_bg_image"
CONF_DISP_BG_OPA = "disp_bg_opa"
CONF_BODY = "body"
CONF_BUTTONS = "buttons"
CONF_CHANGE_RATE = "change_rate"
CONF_CLOSE_BUTTON = "close_button"
CONF_COLOR_DEPTH = "color_depth"
CONF_COLOR_END = "color_end"
CONF_COLOR_START = "color_start"
CONF_CONTAINER = "container"
CONF_CONTROL = "control"
CONF_DEFAULT_FONT = "default_font"
CONF_DEFAULT_GROUP = "default_group"
CONF_DIR = "dir"
CONF_DISPLAYS = "displays"
CONF_EDITING = "editing"
CONF_ENCODERS = "encoders"
CONF_END_ANGLE = "end_angle"
CONF_END_VALUE = "end_value"
CONF_ENTER_BUTTON = "enter_button"
CONF_ENTRIES = "entries"
CONF_EXT_CLICK_AREA = "ext_click_area"
CONF_FLAGS = "flags"
CONF_FLEX_FLOW = "flex_flow"
CONF_FLEX_ALIGN_MAIN = "flex_align_main"
CONF_FLEX_ALIGN_CROSS = "flex_align_cross"
CONF_FLEX_ALIGN_TRACK = "flex_align_track"
CONF_FLEX_GROW = "flex_grow"
CONF_FREEZE = "freeze"
CONF_DARK_MODE = "dark_mode"
CONF_FULL_REFRESH = "full_refresh"
CONF_GRADIENTS = "gradients"
CONF_GRID_CELL_ROW_POS = "grid_cell_row_pos"
CONF_GRID_CELL_COLUMN_POS = "grid_cell_column_pos"
CONF_GRID_CELL_ROW_SPAN = "grid_cell_row_span"
CONF_GRID_CELL_COLUMN_SPAN = "grid_cell_column_span"
CONF_GRID_CELL_X_ALIGN = "grid_cell_x_align"
CONF_GRID_CELL_Y_ALIGN = "grid_cell_y_align"
CONF_GRID_COLUMN_ALIGN = "grid_column_align"
CONF_GRID_COLUMNS = "grid_columns"
CONF_GRID_ROW_ALIGN = "grid_row_align"
CONF_GRID_ROWS = "grid_rows"
CONF_HEADER_BUTTONS = "header_buttons"
CONF_HEADER_MODE = "header_mode"
CONF_HOME = "home"
CONF_INDICATORS = "indicators"
CONF_INITIAL_FOCUS = "initial_focus"
CONF_SELECTED_DIGIT = "selected_digit"
CONF_KEY_CODE = "key_code"
CONF_KEYPADS = "keypads"
CONF_LAYOUT = "layout"
CONF_LEFT_BUTTON = "left_button"
CONF_LINE_WIDTH = "line_width"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_LONG_PRESS_REPEAT_TIME = "long_press_repeat_time"
CONF_LVGL_ID = "lvgl_id"
CONF_LONG_MODE = "long_mode"
CONF_MAJOR_TICKS_STYLE = "major_ticks_style"
CONF_MSGBOXES = "msgboxes"
CONF_OBJ = "obj"
CONF_ONE_CHECKED = "one_checked"
CONF_ONE_LINE = "one_line"
CONF_ON_DRAW_START = "on_draw_start"
CONF_ON_DRAW_END = "on_draw_end"
CONF_ON_PAUSE = "on_pause"
CONF_ON_RESUME = "on_resume"
CONF_ON_SELECT = "on_select"
CONF_OPA = "opa"
CONF_NEXT = "next"
CONF_PAD_ROW = "pad_row"
CONF_PAD_COLUMN = "pad_column"
CONF_PAGE = "page"
CONF_PAGE_WRAP = "page_wrap"
CONF_PASSWORD_MODE = "password_mode"
CONF_PIVOT_X = "pivot_x"
CONF_PIVOT_Y = "pivot_y"
CONF_PLACEHOLDER_TEXT = "placeholder_text"
CONF_POINTS = "points"
CONF_PREVIOUS = "previous"
CONF_RADIUS = "radius"
CONF_REPEAT_COUNT = "repeat_count"
CONF_RECOLOR = "recolor"
CONF_RESUME_ON_INPUT = "resume_on_input"
CONF_RIGHT_BUTTON = "right_button"
CONF_ROLLOVER = "rollover"
CONF_ROOT_BACK_BTN = "root_back_btn"
CONF_ROWS = "rows"
CONF_SCALE = "scale"
CONF_SCALE_LINES = "scale_lines"
CONF_SCROLLBAR_MODE = "scrollbar_mode"
CONF_SCROLL_DIR = "scroll_dir"
CONF_SCROLL_SNAP_X = "scroll_snap_x"
CONF_SCROLL_SNAP_Y = "scroll_snap_y"
CONF_SELECTED_INDEX = "selected_index"
CONF_SELECTED_TEXT = "selected_text"
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
CONF_TAB_ID = "tab_id"
CONF_TABS = "tabs"
CONF_TICK_STYLE = "tick_style"
CONF_TIME_FORMAT = "time_format"
CONF_TILE = "tile"
CONF_TILE_ID = "tile_id"
CONF_TILES = "tiles"
CONF_TITLE = "title"
CONF_TOP_LAYER = "top_layer"
CONF_TOUCHSCREENS = "touchscreens"
CONF_TRANSFORM_ROTATION = "transform_rotation"
CONF_TRANSFORM_SCALE = "transform_scale"
CONF_TRANSPARENCY_KEY = "transparency_key"
CONF_THEME = "theme"
CONF_UPDATE_ON_RELEASE = "update_on_release"
CONF_UPDATE_WHEN_DISPLAY_IDLE = "update_when_display_idle"
CONF_VISIBLE_ROW_COUNT = "visible_row_count"
CONF_WIDGET = "widget"
CONF_WIDGETS = "widgets"
CONF_ZOOM = "zoom"

# Keypad keys

LV_KEYS = LvConstant(
    "LV_KEY_",
    "UP",
    "DOWN",
    "RIGHT",
    "LEFT",
    "ESC",
    "DEL",
    "BACKSPACE",
    "ENTER",
    "NEXT",
    "PREV",
    "HOME",
    "END",
)

LV_SCALE_MODE = LvConstant(
    "LV_SCALE_MODE_",
    "HORIZONTAL_TOP",
    "HORIZONTAL_BOTTOM",
    "VERTICAL_LEFT",
    "VERTICAL_RIGHT",
    "ROUND_INNER",
    "ROUND_OUTER",
)


DEFAULT_ESPHOME_FONT = "esphome_lv_default_font"


def join_enums(enums, prefix=""):
    enums = list(enums)
    enums.sort()
    # If a prefix is provided, prepend each constant with the prefix, and assume that all the constants are within the
    # same namespace, otherwise cast to int to avoid triggering warnings about mixing enum types.
    if prefix:
        return literal("|".join(f"{prefix}{e.upper()}" for e in enums))
    return literal("|".join(f"(int){e.upper()}" for e in enums))


# fmt: off
LV_COLOR_FORMATS = (
    "RGB565", "SWAPPED", "RGB565A8", "RGB888", "XRGB8888", "ARGB8888", "PREMULTIPLIED", "L8", "AL88", "A8", "I1",
)

LV_DEFINES = (
    "LV_USE_FREERTOS_TASK_NOTIFY", "LV_DRAW_BUF_STRIDE_ALIGN", "LV_USE_DRAW_SW", "LV_DRAW_SW_DRAW_UNIT_CNT",
    "LV_DRAW_SW_COMPLEX", "LV_USE_DRAW_PXP", "LV_USE_PXP_DRAW_THREAD", "LV_USE_DRAW_G2D",
    "LV_USE_G2D_DRAW_THREAD", "LV_VG_LITE_USE_BOX_SHADOW", "LV_VG_LITE_THORVG_16PIXELS_ALIGN", "LV_LOG_USE_TIMESTAMP",
    "LV_LOG_USE_FILE_LINE", "LV_USE_OBJ_ID_BUILTIN", "LV_USE_OBJ_PROPERTY_NAME", "LV_ATTRIBUTE_MEM_ALIGN_SIZE",
    "LV_FONT_MONTSERRAT_14", "LV_USE_FONT_PLACEHOLDER", "LV_WIDGETS_HAS_DEFAULT_VALUE", "LV_USE_ARCLABEL",
    "LV_USE_CALENDAR", "LV_USE_CALENDAR_HEADER_ARROW", "LV_USE_CALENDAR_HEADER_DROPDOWN", "LV_USE_CHART",
    "LV_USE_LIST", "LV_USE_MENU", "LV_USE_MSGBOX", "LV_USE_SCALE",
    "LV_USE_TABLE", "LV_USE_SPAN", "LV_USE_WIN", "LV_USE_THEME_DEFAULT",
    "LV_THEME_DEFAULT_GROW", "LV_USE_THEME_SIMPLE", "LV_USE_THEME_MONO", "LV_USE_FLEX",
    "LV_USE_GRID", "LV_USE_PROFILER_BUILTIN", "LV_PROFILER_BUILTIN_DEFAULT_ENABLE", "LV_PROFILER_LAYOUT",
    "LV_PROFILER_REFR", "LV_PROFILER_DRAW", "LV_PROFILER_INDEV", "LV_PROFILER_DECODER",
    "LV_PROFILER_FONT", "LV_PROFILER_FS", "LV_PROFILER_TIMER", "LV_PROFILER_CACHE",
    "LV_PROFILER_EVENT", "LV_USE_OBSERVER", "LV_IME_PINYIN_USE_DEFAULT_DICT", "LV_IME_PINYIN_USE_K9_MODE",
    "LV_FILE_EXPLORER_QUICK_ACCESS", "LV_TEST_SCREENSHOT_CREATE_REFERENCE_IMAGE", "LV_LINUX_FBDEV_MMAP",
    "LV_USE_NUTTX_MOUSE_MOVE_STEP", "LV_USE_GENERIC_MIPI", "LV_BUILD_EXAMPLES", "LV_BUILD_DEMOS",
    "LV_WAYLAND_USE_EGL", "LV_WAYLAND_USE_G2D", "LV_WAYLAND_USE_SHM", "LV_LINUX_DRM_USE_EGL",
    "LV_USE_LZ4", "LV_USE_THORVG", "LV_SDL_USE_EGL", "LV_USE_EGL", "LV_LABEL_LONG_TXT_HINT", "LV_LABEL_TEXT_SELECTION",
) + tuple(f"LV_DRAW_SW_SUPPORT_{f}" for f in LV_COLOR_FORMATS)
