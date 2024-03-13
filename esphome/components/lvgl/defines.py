# Widgets
CONF_ANIMIMG = "animimg"
CONF_ARC = "arc"
CONF_BAR = "bar"
CONF_BTN = "btn"
CONF_BTNMATRIX = "btnmatrix"
CONF_CANVAS = "canvas"
CONF_CHECKBOX = "checkbox"
CONF_DROPDOWN = "dropdown"
CONF_IMG = "img"
CONF_LABEL = "label"
CONF_LINE = "line"
CONF_DROPDOWN_LIST = "dropdown_list"
CONF_METER = "meter"
CONF_ROLLER = "roller"
CONF_SCREEN = "screen"
CONF_PAGE = "page"
CONF_SLIDER = "slider"
CONF_SPINNER = "spinner"
CONF_SWITCH = "switch"
CONF_TABLE = "table"
CONF_TEXTAREA = "textarea"

# Input devices
CONF_ROTARY_ENCODERS = "rotary_encoders"
CONF_TOUCHSCREENS = "touchscreens"

# Parts
CONF_MAIN = "main"
CONF_SCROLLBAR = "scrollbar"
CONF_INDICATOR = "indicator"
CONF_KNOB = "knob"
CONF_SELECTED = "selected"
CONF_ITEMS = "items"
CONF_TICKS = "ticks"
CONF_CURSOR = "cursor"
CONF_TEXTAREA_PLACEHOLDER = "textarea_placeholder"

LV_FONTS = list(map(lambda size: f"montserrat_{size}", range(8, 50, 2))) + [
    "dejavu_16_persian_hebrew",
    "simsun_16_cjk",
    "unscii_8",
    "unscii_16",
]

LV_EVENT = {
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
}

LV_EVENT_TRIGGERS = list(map(lambda x: "on_" + x.lower(), LV_EVENT))

LV_ANIM = (
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

LV_LONG_MODES = (
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

ROLLER_MODES = ("NORMAL", "INFINITE")
DIRECTIONS = ("LEFT", "RIGHT", "BOTTOM", "TOP")
CHILD_ALIGNMENTS = (
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

ALIGN_ALIGNMENTS = CHILD_ALIGNMENTS + (
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

FLEX_FLOWS = (
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

ARC_MODES = ["NORMAL", "REVERSE", "SYMMETRICAL"]
BAR_MODES = ["NORMAL", "SYMMETRICAL", "RANGE"]

BTNMATRIX_CTRLS = (
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

LV_CELL_ALIGNMENTS = (
    "START",
    "CENTER",
    "END",
)
LV_GRID_ALIGNMENTS = LV_CELL_ALIGNMENTS + (
    "STRETCH",
    "SPACE_EVENLY",
    "SPACE_AROUND",
    "SPACE_BETWEEN",
)
