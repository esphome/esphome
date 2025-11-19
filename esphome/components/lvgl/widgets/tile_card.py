"""
Tile Card Widget - A Home Assistant style tile card for LVGL.

Displays a modern tile-style widget with icon, name, and state.
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
    literal,
    lvgl_ns,
)
from ..helpers import add_lv_use
from ..lv_validation import lv_bool, lv_color, lv_image, lv_text
from ..lvcode import lv, lv_add, lv_obj
from ..types import LvBoolean, LvCompound, WidgetType
from . import Widget

CONF_TILE_CARD = "tile_card"
CONF_ICON_COLOR = "icon_color"
CONF_ICON_ON_COLOR = "icon_on_color"
CONF_STATE_TEXT = "state_text"
CONF_VERTICAL = "vertical"

# Reference to C++ class
LvTileCardType = lvgl_ns.class_("LvTileCardType", LvCompound)

TILE_CARD_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ICON): lv_image,
        cv.Optional(CONF_NAME): lv_text,
        cv.Optional(CONF_STATE, default=False): lv_bool,
        cv.Optional(CONF_STATE_TEXT): lv_text,
        cv.Optional(CONF_ICON_COLOR, default=0xFFFFFF): lv_color,
        cv.Optional(CONF_ICON_ON_COLOR, default=0xFFD700): lv_color,
        cv.Optional(CONF_VERTICAL, default=False): cv.boolean,
    }
)

TILE_CARD_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_STATE): lv_bool,
        cv.Optional(CONF_STATE_TEXT): lv_text,
        cv.Optional(CONF_NAME): lv_text,
    }
)

# LvType wrapper for the tile card
lv_tile_card_t = LvBoolean("LvTileCardType", parents=(LvCompound,))


class TileCardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_TILE_CARD,
            lv_tile_card_t,
            (CONF_MAIN,),
            TILE_CARD_SCHEMA,
            TILE_CARD_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return ("obj", "label", "img")

    async def to_code(self, w: Widget, config):
        """Generate code for the tile card widget."""
        add_lv_use("obj", "label", "img")

        var = w.var
        obj = w.obj

        # Only set up structure on initial creation
        if CONF_ICON in config or CONF_NAME in config:
            # Set up the container with flex layout
            lv_obj.add_flag(obj, literal("LV_OBJ_FLAG_CLICKABLE"))

            # Choose layout direction based on vertical setting
            if config.get(CONF_VERTICAL, False):
                lv_obj.set_flex_flow(obj, literal("LV_FLEX_FLOW_COLUMN"))
            else:
                lv_obj.set_flex_flow(obj, literal("LV_FLEX_FLOW_ROW"))

            lv_obj.set_flex_align(
                obj,
                literal("LV_FLEX_ALIGN_START"),
                literal("LV_FLEX_ALIGN_CENTER"),
                literal("LV_FLEX_ALIGN_CENTER"),
            )

            # Set some padding
            lv.obj_set_style_pad_all(obj, 10, literal("LV_PART_MAIN"))
            lv.obj_set_style_pad_gap(obj, 10, literal("LV_PART_MAIN"))

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

            # Create name label
            if name := config.get(CONF_NAME):
                lv_add(var.create_name_label())
                name_label = var.get_name_label()
                name_text = await lv_text.process(name)
                lv.label_set_text(name_label, name_text)

            # Create state label if state_text is provided
            if state_text := config.get(CONF_STATE_TEXT):
                lv_add(var.create_state_label())
                state_label = var.get_state_label()
                state_txt = await lv_text.process(state_text)
                lv.label_set_text(state_label, state_txt)

        # Set the state
        if (state := config.get(CONF_STATE)) is not None:
            state_val = await lv_bool.process(state)
            lv_add(var.set_state(state_val))

        # Update state text if provided during update
        if CONF_STATE_TEXT in config and CONF_ICON not in config:
            state_text = await lv_text.process(config[CONF_STATE_TEXT])
            state_label = var.get_state_label()
            lv.label_set_text(state_label, state_text)


tile_card_spec = TileCardType()
