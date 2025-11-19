"""
Thermostat Card Widget - A Home Assistant style thermostat card for LVGL.

Displays a climate control interface with temperature display, setpoint control,
and mode buttons.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_NAME,
    CONF_STEP,
)

from ..defines import (
    CONF_INDICATOR,
    CONF_KNOB,
    CONF_MAIN,
    literal,
    lvgl_ns,
)
from ..helpers import add_lv_use, lvgl_components_required
from ..lv_validation import lv_color, lv_float, lv_text
from ..lvcode import lv, lv_add, lv_obj
from ..types import LvCompound, LvNumber, WidgetType
from . import Widget

CONF_THERMOSTAT_CARD = "thermostat_card"
CONF_CURRENT_TEMPERATURE = "current_temperature"
CONF_TARGET_TEMPERATURE = "target_temperature"
CONF_SHOW_CURRENT = "show_current"
CONF_SHOW_SETPOINT = "show_setpoint"
CONF_SHOW_BUTTONS = "show_buttons"
CONF_ARC_COLOR = "arc_color"
CONF_HEATING_COLOR = "heating_color"
CONF_COOLING_COLOR = "cooling_color"
CONF_UNIT = "unit"

# Reference to C++ class
LvThermostatCardType = lvgl_ns.class_("LvThermostatCardType", LvCompound)

THERMOSTAT_CARD_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NAME): lv_text,
        cv.Optional(CONF_CURRENT_TEMPERATURE): lv_float,
        cv.Optional(CONF_TARGET_TEMPERATURE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=5.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=35.0): cv.float_,
        cv.Optional(CONF_STEP, default=0.5): cv.float_,
        cv.Optional(CONF_SHOW_CURRENT, default=True): cv.boolean,
        cv.Optional(CONF_SHOW_SETPOINT, default=True): cv.boolean,
        cv.Optional(CONF_SHOW_BUTTONS, default=True): cv.boolean,
        cv.Optional(CONF_ARC_COLOR, default=0x3498DB): lv_color,
        cv.Optional(CONF_HEATING_COLOR, default=0xE74C3C): lv_color,
        cv.Optional(CONF_COOLING_COLOR, default=0x3498DB): lv_color,
        cv.Optional(CONF_UNIT, default="°"): cv.string,
    }
)

THERMOSTAT_CARD_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CURRENT_TEMPERATURE): lv_float,
        cv.Optional(CONF_TARGET_TEMPERATURE): lv_float,
    }
)

# LvType wrapper for the thermostat card
lv_thermostat_card_t = LvNumber("LvThermostatCardType", parents=(LvCompound,))


class ThermostatCardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_THERMOSTAT_CARD,
            lv_thermostat_card_t,
            (CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
            THERMOSTAT_CARD_SCHEMA,
            THERMOSTAT_CARD_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return ("arc", "label", "btn", "obj")

    def get_min(self, config: dict):
        return int(config.get(CONF_MIN_VALUE, 5) * 10)

    def get_max(self, config: dict):
        return int(config.get(CONF_MAX_VALUE, 35) * 10)

    async def to_code(self, w: Widget, config):
        """Generate code for the thermostat card widget."""
        lvgl_components_required.add("arc")
        add_lv_use("arc", "label", "btn")

        var = w.var
        obj = w.obj

        # Get configuration values
        min_val = config.get(CONF_MIN_VALUE, 5.0)
        max_val = config.get(CONF_MAX_VALUE, 35.0)
        unit = config.get(CONF_UNIT, "°")

        # Only set up structure on initial creation
        if CONF_MIN_VALUE in config:
            # Set up the container with flex layout
            lv_obj.set_flex_flow(obj, literal("LV_FLEX_FLOW_COLUMN"))
            lv_obj.set_flex_align(
                obj,
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
            )

            # Create the temperature arc
            lv_add(var.create_temperature_arc())
            temp_arc = var.get_temperature_arc()

            # Configure arc - use 10x values for precision
            lv.arc_set_range(temp_arc, int(min_val * 10), int(max_val * 10))
            lv.arc_set_bg_angles(temp_arc, 135, 45)
            lv.arc_set_rotation(temp_arc, 0)
            lv.arc_set_mode(temp_arc, literal("LV_ARC_MODE_NORMAL"))

            # Set arc color
            if arc_color := config.get(CONF_ARC_COLOR):
                color = await lv_color.process(arc_color)
                lv.obj_set_style_arc_color(temp_arc, color, literal("LV_PART_INDICATOR"))

            # Make arc interactive
            lv_obj.add_flag(temp_arc, literal("LV_OBJ_FLAG_CLICKABLE"))

            # Create current temperature label
            if config.get(CONF_SHOW_CURRENT, True):
                lv_add(var.create_temperature_label())
                temp_label = var.get_temperature_label()
                lv_obj.align(temp_label, literal("LV_ALIGN_CENTER"), 0, -20)
                lv.label_set_text(temp_label, literal(f'"--{unit}"'))

            # Create setpoint label
            if config.get(CONF_SHOW_SETPOINT, True):
                lv_add(var.create_setpoint_label())
                setpoint_label = var.get_setpoint_label()
                lv_obj.align(setpoint_label, literal("LV_ALIGN_CENTER"), 0, 20)
                lv.label_set_text(setpoint_label, literal(f'"--{unit}"'))

            # Create up/down buttons
            if config.get(CONF_SHOW_BUTTONS, True):
                lv_add(var.create_up_button())
                up_btn = var.get_up_button()
                lv_obj.align(up_btn, literal("LV_ALIGN_RIGHT_MID"), -10, -30)
                lv_obj.set_size(up_btn, 40, 40)

                lv_add(var.create_down_button())
                down_btn = var.get_down_button()
                lv_obj.align(down_btn, literal("LV_ALIGN_RIGHT_MID"), -10, 30)
                lv_obj.set_size(down_btn, 40, 40)

        # Set current temperature
        if (current_temp := config.get(CONF_CURRENT_TEMPERATURE)) is not None:
            temp_val = await lv_float.process(current_temp)
            lv_add(var.set_current_temperature(temp_val))

        # Set target temperature
        if (target_temp := config.get(CONF_TARGET_TEMPERATURE)) is not None:
            target_val = await lv_float.process(target_temp)
            lv_add(var.set_target_temperature(target_val))


thermostat_card_spec = ThermostatCardType()
