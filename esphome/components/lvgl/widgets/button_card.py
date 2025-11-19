"""
Button Card Widget - A Home Assistant style button card for LVGL.

Displays a button with an icon and label, with optional state indicator.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    CONF_STATE,
)

from ..defines import (
    CONF_MAIN,
    CONF_SRC,
    literal,
    lvgl_ns,
)
from ..helpers import add_lv_use, lvgl_components_required
from ..lv_validation import lv_bool, lv_color, lv_image, lv_text
from ..lvcode import lv, lv_add, lv_obj
from ..types import LvBoolean, LvCompound, WidgetType
from . import Widget

CONF_BUTTON_CARD = "button_card"
CONF_ICON_COLOR = "icon_color"
CONF_ICON_ON_COLOR = "icon_on_color"
CONF_SHOW_STATE = "show_state"
CONF_STATE_TEXT_ON = "state_text_on"
CONF_STATE_TEXT_OFF = "state_text_off"

# Reference to C++ class
LvButtonCardType = lvgl_ns.class_("LvButtonCardType", LvCompound)

BUTTON_CARD_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ICON): lv_image,
        cv.Optional(CONF_NAME): lv_text,
        cv.Optional(CONF_STATE, default=False): lv_bool,
        cv.Optional(CONF_ICON_COLOR, default=0xFFFFFF): lv_color,
        cv.Optional(CONF_ICON_ON_COLOR, default=0xFFD700): lv_color,
        cv.Optional(CONF_SHOW_STATE, default=False): cv.boolean,
        cv.Optional(CONF_STATE_TEXT_ON, default="On"): cv.string,
        cv.Optional(CONF_STATE_TEXT_OFF, default="Off"): cv.string,
    }
)

BUTTON_CARD_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_STATE): lv_bool,
        cv.Optional(CONF_NAME): lv_text,
    }
)

# LvType wrapper for the button card
lv_button_card_t = LvBoolean("LvButtonCardType", parents=(LvCompound,))


class ButtonCardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_BUTTON_CARD,
            lv_button_card_t,
            (CONF_MAIN,),
            BUTTON_CARD_SCHEMA,
            BUTTON_CARD_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return ("btn", "label", "img")

    async def to_code(self, w: Widget, config):
        """Generate code for the button card widget."""
        add_lv_use("btn", "label", "img")

        var = w.var
        obj = w.obj

        # Only set up structure on initial creation
        if CONF_ICON in config or CONF_NAME in config:
            # Set up the container as a button with flex layout
            lv_obj.add_flag(obj, literal("LV_OBJ_FLAG_CLICKABLE"))
            lv_obj.set_flex_flow(obj, literal("LV_FLEX_FLOW_COLUMN"))
            lv_obj.set_flex_align(
                obj,
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
            )

            # Create icon if provided
            if icon := config.get(CONF_ICON):
                lv_add(var.create_icon())
                icon_obj = var.get_icon()
                icon_src = await lv_image.process(icon)
                lv.img_set_src(icon_obj, icon_src)

                # Set icon color
                if icon_color := config.get(CONF_ICON_COLOR):
                    color = await lv_color.process(icon_color)
                    lv.obj_set_style_img_recolor(icon_obj, color, literal("LV_PART_MAIN"))
                    lv.obj_set_style_img_recolor_opa(
                        icon_obj, literal("LV_OPA_COVER"), literal("LV_PART_MAIN")
                    )

            # Create label if name is provided
            if name := config.get(CONF_NAME):
                lv_add(var.create_label())
                label_obj = var.get_label()
                name_text = await lv_text.process(name)
                lv.label_set_text(label_obj, name_text)

            # Create state label if show_state is enabled
            if config.get(CONF_SHOW_STATE, False):
                lv_add(var.create_state_label())
                state_label = var.get_state_label()
                # Set initial state text
                state_text = (
                    config.get(CONF_STATE_TEXT_OFF, "Off")
                )
                lv.label_set_text(state_label, literal(f'"{state_text}"'))

        # Set the state
        if (state := config.get(CONF_STATE)) is not None:
            state_val = await lv_bool.process(state)
            lv_add(var.set_state(state_val))

            # Update state label if shown
            if config.get(CONF_SHOW_STATE, False):
                state_label = var.get_state_label()
                # This would need to be conditional at runtime
                # For now, just set the text based on config


button_card_spec = ButtonCardType()
