"""
Gauge Card Widget - A Home Assistant style gauge card for LVGL.

Displays a value in a circular arc gauge with optional value label in the center.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VALUE,
)

from ..defines import (
    CONF_END_ANGLE,
    CONF_INDICATOR,
    CONF_KNOB,
    CONF_MAIN,
    CONF_START_ANGLE,
    CONF_WIDGETS,
    literal,
    lvgl_ns,
)
from ..helpers import add_lv_use, lvgl_components_required
from ..lv_validation import lv_color, lv_float, lv_int, lv_text, size
from ..lvcode import LocalVariable, lv, lv_add, lv_assign, lv_expr, lv_obj
from ..types import LvCompound, LvNumber, LvType, WidgetType, lv_obj_t_ptr
from . import Widget, add_widgets, set_obj_properties, widget_to_code
from .label import CONF_LABEL

CONF_GAUGE_CARD = "gauge_card"

# Reference to C++ class
LvGaugeCardType = lvgl_ns.class_("LvGaugeCardType", LvCompound)
CONF_ARC_COLOR = "arc_color"
CONF_BACKGROUND_ARC_COLOR = "background_arc_color"
CONF_SHOW_VALUE = "show_value"
CONF_SHOW_NAME = "show_name"
CONF_NEEDLE_COLOR = "needle_color"
CONF_VALUE_FONT = "value_font"
CONF_NAME_FONT = "name_font"
CONF_SEVERITY = "severity"

# Severity levels for color changes
SEVERITY_SCHEMA = cv.Schema(
    {
        cv.Optional("green"): cv.float_,
        cv.Optional("yellow"): cv.float_,
        cv.Optional("red"): cv.float_,
    }
)

GAUGE_CARD_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): lv_int,
        cv.Optional(CONF_MAX_VALUE, default=100): lv_int,
        cv.Optional(CONF_START_ANGLE, default=135): cv.int_range(0, 360),
        cv.Optional(CONF_END_ANGLE, default=45): cv.int_range(0, 360),
        cv.Optional(CONF_NAME): lv_text,
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=""): cv.string,
        cv.Optional(CONF_FORMAT, default="%.0f"): cv.string,
        cv.Optional(CONF_ARC_COLOR, default=0x3498DB): lv_color,
        cv.Optional(CONF_BACKGROUND_ARC_COLOR, default=0x404040): lv_color,
        cv.Optional(CONF_SHOW_VALUE, default=True): cv.boolean,
        cv.Optional(CONF_SHOW_NAME, default=True): cv.boolean,
        cv.Optional(CONF_SEVERITY): SEVERITY_SCHEMA,
    }
)

GAUGE_CARD_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
    }
)


# LvType wrapper for the gauge card - uses LvNumber for value handling
lv_gauge_card_t = LvNumber("LvGaugeCardType", parents=(LvCompound,))


class GaugeCardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_GAUGE_CARD,
            lv_gauge_card_t,
            (CONF_MAIN, CONF_INDICATOR),
            GAUGE_CARD_SCHEMA,
            GAUGE_CARD_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return ("arc", "label", "obj")

    async def to_code(self, w: Widget, config):
        """Generate code for the gauge card widget."""
        lvgl_components_required.add("arc")
        add_lv_use("arc", "label")

        var = w.var
        obj = w.obj

        # Get configuration values
        min_val = config.get(CONF_MIN_VALUE, 0)
        max_val = config.get(CONF_MAX_VALUE, 100)
        start_angle = config.get(CONF_START_ANGLE, 135)
        end_angle = config.get(CONF_END_ANGLE, 45)

        # Only set up the arc structure on initial creation
        if CONF_MIN_VALUE in config:
            # Set up the container styling for the card
            lv_obj.set_flex_flow(obj, literal("LV_FLEX_FLOW_COLUMN"))
            lv_obj.set_flex_align(
                obj,
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
            )

            # Create the main arc gauge
            arc_id = f"{config[CONF_ID]}_arc"
            lv_add(var.create_arc())

            arc_obj = var.get_arc()

            # Configure arc
            lv.arc_set_range(arc_obj, min_val, max_val)
            lv.arc_set_bg_angles(arc_obj, start_angle, end_angle)
            lv.arc_set_rotation(arc_obj, 0)
            lv.arc_set_mode(arc_obj, literal("LV_ARC_MODE_NORMAL"))

            # Remove the knob for display-only gauge
            lv_obj.remove_style(arc_obj, literal("NULL"), literal("LV_PART_KNOB"))
            lv_obj.clear_flag(arc_obj, literal("LV_OBJ_FLAG_CLICKABLE"))

            # Set arc colors
            if arc_color := config.get(CONF_ARC_COLOR):
                color = await lv_color.process(arc_color)
                lv.obj_set_style_arc_color(arc_obj, color, literal("LV_PART_INDICATOR"))

            if bg_arc_color := config.get(CONF_BACKGROUND_ARC_COLOR):
                bg_color = await lv_color.process(bg_arc_color)
                lv.obj_set_style_arc_color(arc_obj, bg_color, literal("LV_PART_MAIN"))

            # Create value label in center if enabled
            if config.get(CONF_SHOW_VALUE, True):
                lv_add(var.create_value_label())
                value_label = var.get_value_label()
                lv_obj.set_align(value_label, literal("LV_ALIGN_CENTER"))
                lv.label_set_text(value_label, literal('"--"'))

            # Create name label below if enabled and name is provided
            if config.get(CONF_SHOW_NAME, True) and config.get(CONF_NAME):
                lv_add(var.create_name_label())
                name_label = var.get_name_label()
                name_text = await lv_text.process(config[CONF_NAME])
                lv.label_set_text(name_label, name_text)

        # Set the value
        if (value := config.get(CONF_VALUE)) is not None:
            value_processed = await lv_float.process(value)
            lv_add(var.set_value(value_processed))

            # Update the value label text
            if config.get(CONF_SHOW_VALUE, True):
                fmt = config.get(CONF_FORMAT, "%.0f")
                unit = config.get(CONF_UNIT_OF_MEASUREMENT, "")
                # Format string for the value display
                format_str = f'"{fmt}{unit}"'
                lv_add(var.update_value_label(value_processed, literal(format_str)))


gauge_card_spec = GaugeCardType()
