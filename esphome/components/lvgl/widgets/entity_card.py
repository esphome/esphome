"""
Entity Card Widget - A Home Assistant style entity card for LVGL.

Displays an entity with icon, name, and value in a simple horizontal layout.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    CONF_VALUE,
)

from ..defines import (
    CONF_MAIN,
    literal,
    lvgl_ns,
)
from ..helpers import add_lv_use
from ..lv_validation import lv_color, lv_image, lv_text
from ..lvcode import lv, lv_add, lv_obj
from ..types import LvCompound, LvText, WidgetType
from . import Widget

CONF_ENTITY_CARD = "entity_card"
CONF_ICON_COLOR = "icon_color"

# Reference to C++ class
LvEntityCardType = lvgl_ns.class_("LvEntityCardType", LvCompound)

ENTITY_CARD_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ICON): lv_image,
        cv.Optional(CONF_NAME): lv_text,
        cv.Optional(CONF_VALUE): lv_text,
        cv.Optional(CONF_ICON_COLOR, default=0x3498DB): lv_color,
    }
)

ENTITY_CARD_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_text,
        cv.Optional(CONF_NAME): lv_text,
    }
)

# LvType wrapper for the entity card
lv_entity_card_t = LvText("LvEntityCardType", parents=(LvCompound,))


class EntityCardType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ENTITY_CARD,
            lv_entity_card_t,
            (CONF_MAIN,),
            ENTITY_CARD_SCHEMA,
            ENTITY_CARD_MODIFY_SCHEMA,
        )

    def get_uses(self):
        return ("obj", "label", "img")

    async def to_code(self, w: Widget, config):
        """Generate code for the entity card widget."""
        add_lv_use("obj", "label", "img")

        var = w.var
        obj = w.obj

        # Only set up structure on initial creation
        if CONF_ICON in config or CONF_NAME in config or CONF_VALUE in config:
            # Set up horizontal flex layout
            lv_obj.set_flex_flow(obj, literal("LV_FLEX_FLOW_ROW"))
            lv_obj.set_flex_align(
                obj,
                literal("LV_FLEX_ALIGN_SPACE_BETWEEN"),
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
                # Make label take available space
                lv_obj.set_flex_grow(name_label, 1)

            # Create value label
            if value := config.get(CONF_VALUE):
                lv_add(var.create_value_label())
                value_label = var.get_value_label()
                value_text = await lv_text.process(value)
                lv.label_set_text(value_label, value_text)

        # Update value during modify
        if CONF_VALUE in config and CONF_ICON not in config:
            value_text = await lv_text.process(config[CONF_VALUE])
            value_label = var.get_value_label()
            lv.label_set_text(value_label, value_text)

        # Update name during modify
        if CONF_NAME in config and CONF_ICON not in config:
            name_text = await lv_text.process(config[CONF_NAME])
            name_label = var.get_name_label()
            lv.label_set_text(name_label, name_text)


entity_card_spec = EntityCardType()
