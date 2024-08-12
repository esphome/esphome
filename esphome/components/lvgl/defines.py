"""
This is the base of the import tree for LVGL. It contains constant definitions used elsewhere.
Constants already defined in esphome.const are not duplicated here and must be imported where used.

"""

from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_ITEMS
from esphome.core import ID, Lambda
from esphome.cpp_generator import MockObj
from esphome.cpp_types import uint32
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from .helpers import requires_component

lvgl_ns = cg.esphome_ns.namespace("lvgl")


def literal(arg):
    if isinstance(arg, str):
        return MockObj(arg)
    return arg


class LValidator:
    """
    A validator for a particular type used in LVGL. Usable in configs as a validator, also
    has `process()` to convert a value during code generation
    """

    def __init__(
        self, validator, rtype, idtype=None, idexpr=None, retmapper=None, requires=None
    ):
        self.validator = validator
        self.rtype = rtype
        self.idtype = idtype
        self.idexpr = idexpr
        self.retmapper = retmapper
        self.requires = requires

    def __call__(self, value):
        if self.requires:
            value = requires_component(self.requires)(value)
        if isinstance(value, cv.Lambda):
            return cv.returning_lambda(value)
        if self.idtype is not None and isinstance(value, ID):
            return cv.use_id(self.idtype)(value)
        return self.validator(value)

    async def process(self, value, args=()):
        if value is None:
            return None
        if isinstance(value, Lambda):
            return cg.RawExpression(
                f"{await cg.process_lambda(value, args, return_type=self.rtype)}()"
            )
        if self.idtype is not None and isinstance(value, ID):
            return cg.RawExpression(f"{value}->{self.idexpr}")
        if self.retmapper is not None:
            return self.retmapper(value)
        return cg.safe_exp(value)


class LvConstant(LValidator):
    """
    Allow one of a list of choices, mapped to upper case, and prepend the choice with the prefix.
    It's also permitted to include the prefix in the value
    The property `one_of` has the single case validator, and `several_of` allows a list of constants.
    """

    def __init__(self, prefix: str, *choices):
        self.prefix = prefix
        self.choices = choices
        prefixed_choices = [prefix + v for v in choices]
        prefixed_validator = cv.one_of(*prefixed_choices, upper=True)

        @schema_extractor("one_of")
        def validator(value):
            if value == SCHEMA_EXTRACT:
                return self.choices
            if isinstance(value, str) and value.startswith(self.prefix):
                return prefixed_validator(value)
            return self.prefix + cv.one_of(*choices, upper=True)(value)

        super().__init__(validator, rtype=uint32)
        self.retmapper = self.mapper
        self.one_of = LValidator(validator, uint32, retmapper=self.mapper)
        self.several_of = LValidator(
            cv.ensure_list(self.one_of), uint32, retmapper=self.mapper
        )

    def mapper(self, value, args=()):
        if not isinstance(value, list):
            value = [value]
        return literal(
            "|".join(
                [
                    str(v) if str(v).startswith(self.prefix) else self.prefix + str(v)
                    for v in value
                ]
            ).upper()
        )

    def extend(self, *choices):
        """
        Extend an LVCconstant with additional choices.
        :param choices: The extra choices
        :return: A new LVConstant instance
        """
        return LvConstant(self.prefix, *(self.choices + choices))


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

LV_FONTS = list(f"montserrat_{s}" for s in range(8, 50, 2)) + [
    "dejavu_16_persian_hebrew",
    "simsun_16_cjk",
    "unscii_8",
    "unscii_16",
]

LV_EVENT_MAP = {
    "PRESS": "PRESSED",
    "SHORT_CLICK": "SHORT_CLICKED",
    "LONG_PRESS": "LONG_PRESSED",
    "LONG_PRESS_REPEAT": "LONG_PRESSED_REPEAT",
    "CLICK": "CLICKED",
    "RELEASE": "RELEASED",
    "SCROLL_BEGIN": "SCROLL_BEGIN",
    "SCROLL_END": "SCROLL_END",
    "SCROLL": "SCROLL",
    "FOCUS": "FOCUSED",
    "DEFOCUS": "DEFOCUSED",
    "READY": "READY",
    "CANCEL": "CANCEL",
}

LV_EVENT_TRIGGERS = tuple(f"on_{x.lower()}" for x in LV_EVENT_MAP)


LV_ANIM = LvConstant(
    "LV_SCR_LOAD_ANIM_",
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

LOG_LEVELS = (
    "TRACE",
    "INFO",
    "WARN",
    "ERROR",
    "USER",
    "NONE",
)

LV_LONG_MODES = LvConstant(
    "LV_LABEL_LONG_",
    "WRAP",
    "DOT",
    "SCROLL",
    "SCROLL_CIRCULAR",
    "CLIP",
)

STATES = (
    "default",
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
    CONF_TICKS,
    CONF_CURSOR,
    CONF_TEXTAREA_PLACEHOLDER,
)

KEYBOARD_MODES = LvConstant(
    "LV_KEYBOARD_MODE_",
    "TEXT_LOWER",
    "TEXT_UPPER",
    "SPECIAL",
    "NUMBER",
)
ROLLER_MODES = LvConstant("LV_ROLLER_MODE_", "NORMAL", "INFINITE")
DIRECTIONS = LvConstant("LV_DIR_", "LEFT", "RIGHT", "BOTTOM", "TOP")
TILE_DIRECTIONS = DIRECTIONS.extend("HOR", "VER", "ALL")
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
    "widget_1",
    "widget_2",
    "user_1",
    "user_2",
    "user_3",
    "user_4",
)

ARC_MODES = LvConstant("LV_ARC_MODE_", "NORMAL", "REVERSE", "SYMMETRICAL")
BAR_MODES = LvConstant("LV_BAR_MODE_", "NORMAL", "SYMMETRICAL", "RANGE")

BUTTONMATRIX_CTRLS = LvConstant(
    "LV_BTNMATRIX_CTRL_",
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
CONF_ANIMATED = "animated"
CONF_ANIMATION = "animation"
CONF_ANTIALIAS = "antialias"
CONF_ARC_LENGTH = "arc_length"
CONF_AUTO_START = "auto_start"
CONF_BACKGROUND_STYLE = "background_style"
CONF_DECIMAL_PLACES = "decimal_places"
CONF_COLUMN = "column"
CONF_DIGITS = "digits"
CONF_DISP_BG_COLOR = "disp_bg_color"
CONF_DISP_BG_IMAGE = "disp_bg_image"
CONF_BODY = "body"
CONF_BUTTONS = "buttons"
CONF_BYTE_ORDER = "byte_order"
CONF_CHANGE_RATE = "change_rate"
CONF_CLOSE_BUTTON = "close_button"
CONF_COLOR_DEPTH = "color_depth"
CONF_CONTROL = "control"
CONF_DEFAULT = "default"
CONF_DEFAULT_FONT = "default_font"
CONF_DEFAULT_GROUP = "default_group"
CONF_DIR = "dir"
CONF_DISPLAYS = "displays"
CONF_ENCODERS = "encoders"
CONF_END_ANGLE = "end_angle"
CONF_END_VALUE = "end_value"
CONF_ENTER_BUTTON = "enter_button"
CONF_ENTRIES = "entries"
CONF_FLAGS = "flags"
CONF_FLEX_FLOW = "flex_flow"
CONF_FLEX_ALIGN_MAIN = "flex_align_main"
CONF_FLEX_ALIGN_CROSS = "flex_align_cross"
CONF_FLEX_ALIGN_TRACK = "flex_align_track"
CONF_FLEX_GROW = "flex_grow"
CONF_FULL_REFRESH = "full_refresh"
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
CONF_HEADER_MODE = "header_mode"
CONF_HOME = "home"
CONF_KEY_CODE = "key_code"
CONF_LAYOUT = "layout"
CONF_LEFT_BUTTON = "left_button"
CONF_LINE_WIDTH = "line_width"
CONF_LOG_LEVEL = "log_level"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_LONG_PRESS_REPEAT_TIME = "long_press_repeat_time"
CONF_LVGL_ID = "lvgl_id"
CONF_LONG_MODE = "long_mode"
CONF_MSGBOXES = "msgboxes"
CONF_OBJ = "obj"
CONF_OFFSET_X = "offset_x"
CONF_OFFSET_Y = "offset_y"
CONF_ONE_LINE = "one_line"
CONF_ON_SELECT = "on_select"
CONF_ONE_CHECKED = "one_checked"
CONF_NEXT = "next"
CONF_PAGE = "page"
CONF_PAGE_WRAP = "page_wrap"
CONF_PASSWORD_MODE = "password_mode"
CONF_PIVOT_X = "pivot_x"
CONF_PIVOT_Y = "pivot_y"
CONF_PLACEHOLDER_TEXT = "placeholder_text"
CONF_POINTS = "points"
CONF_PREVIOUS = "previous"
CONF_REPEAT_COUNT = "repeat_count"
CONF_RECOLOR = "recolor"
CONF_RIGHT_BUTTON = "right_button"
CONF_ROLLOVER = "rollover"
CONF_ROOT_BACK_BTN = "root_back_btn"
CONF_ROWS = "rows"
CONF_SCALE_LINES = "scale_lines"
CONF_SCROLLBAR_MODE = "scrollbar_mode"
CONF_SELECTED_INDEX = "selected_index"
CONF_SHOW_SNOW = "show_snow"
CONF_SPIN_TIME = "spin_time"
CONF_SRC = "src"
CONF_START_ANGLE = "start_angle"
CONF_START_VALUE = "start_value"
CONF_STATES = "states"
CONF_STYLE = "style"
CONF_STYLES = "styles"
CONF_STYLE_DEFINITIONS = "style_definitions"
CONF_STYLE_ID = "style_id"
CONF_SKIP = "skip"
CONF_SYMBOL = "symbol"
CONF_TAB_ID = "tab_id"
CONF_TABS = "tabs"
CONF_TILE = "tile"
CONF_TILE_ID = "tile_id"
CONF_TILES = "tiles"
CONF_TITLE = "title"
CONF_TOP_LAYER = "top_layer"
CONF_TOUCHSCREENS = "touchscreens"
CONF_TRANSPARENCY_KEY = "transparency_key"
CONF_THEME = "theme"
CONF_VISIBLE_ROW_COUNT = "visible_row_count"
CONF_WIDGET = "widget"
CONF_WIDGETS = "widgets"
CONF_X = "x"
CONF_Y = "y"
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


DEFAULT_ESPHOME_FONT = "esphome_lv_default_font"


def join_enums(enums, prefix=""):
    return literal("|".join(f"(int){prefix}{e.upper()}" for e in enums))
