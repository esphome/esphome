import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, font, color
from esphome.const import CONF_ID
from esphome import automation, core

from esphome.components.display_menu_base import (
    DISPLAY_MENU_BASE_SCHEMA,
    DisplayMenuComponent,
    display_menu_to_code,
)

CONF_DISPLAY = "display"
CONF_DISPLAY_UPDATER = "display_updater"
CONF_FONT = "font"
CONF_MENU_ITEM_VALUE = "menu_item_value"
CONF_FOREGROUND_COLOR = "foreground_color"
CONF_BACKGROUND_COLOR = "background_color"

graphical_display_menu = cg.esphome_ns.namespace("graphical_display_menu")
GraphicalDisplayMenu = graphical_display_menu.class_(
    "GraphicalDisplayMenu", DisplayMenuComponent
)

display_menu_base_ns = cg.esphome_ns.namespace("display_menu_base")
MenuItem = display_menu_base_ns.class_("MenuItem")
MenuItemConstPtr = MenuItem.operator("ptr").operator("const")

CODEOWNERS = ["@MrMDavidson"]

AUTO_LOAD = ["display_menu_base"]

DEFAULT_MENU_ITEM_VALUE = """
    std::string label = "(";
    label.append(it->get_value_text());
    label.append(")");
    return label;
"""

CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GraphicalDisplayMenu),
            cv.Required(CONF_DISPLAY): cv.use_id(display.DisplayBuffer),
            cv.Optional(CONF_DISPLAY_UPDATER): cv.use_id(cg.PollingComponent),
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(
                CONF_MENU_ITEM_VALUE, default=DEFAULT_MENU_ITEM_VALUE
            ): cv.templatable(cv.string),
            cv.Optional(CONF_FOREGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_BACKGROUND_COLOR): cv.use_id(color.ColorStruct),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    display_buffer = await cg.get_variable(config[CONF_DISPLAY])
    cg.add(var.set_display_buffer(display_buffer))

    if CONF_DISPLAY_UPDATER in config:
        display_updater = await cg.get_variable(config[CONF_DISPLAY_UPDATER])
        cg.add(var.set_display_updater(display_updater))

    menu_font = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(menu_font))

    if CONF_MENU_ITEM_VALUE in config:
        if isinstance(config[CONF_MENU_ITEM_VALUE], core.Lambda):
            template_ = await cg.templatable(
                config[CONF_MENU_ITEM_VALUE], [(MenuItemConstPtr, "it")], cg.std_string
            )
            cg.add(var.set_menu_item_value(template_))
        else:
            cg.add(var.set_menu_item_value(config[CONF_MENU_ITEM_VALUE]))

    if CONF_FOREGROUND_COLOR in config:
        foreground_color = await cg.get_variable(config[CONF_FOREGROUND_COLOR])
        cg.add(var.set_foreground_color(foreground_color))

    if CONF_BACKGROUND_COLOR in config:
        background_color = await cg.get_variable(config[CONF_BACKGROUND_COLOR])
        cg.add(var.set_background_color(background_color))

    await display_menu_to_code(var, config)
