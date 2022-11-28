import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, font
from esphome.const import CONF_ID

from esphome.components.display_menu_base import (
    DISPLAY_MENU_BASE_SCHEMA,
    DisplayMenuComponent,
    display_menu_to_code,
)

CONF_DISPLAY = "display"
CONF_DISPLAY_UPDATER = "display_updater"
CONF_FONT = "font"


graphical_display_menu = cg.esphome_ns.namespace("graphical_display_menu")
GraphicalDisplayMenu = graphical_display_menu.class_(
    "GraphicalDisplayMenu", DisplayMenuComponent
)

CODEOWNERS = ["@mrmdavidson"]

AUTO_LOAD = ["display_menu_base"]

CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GraphicalDisplayMenu),
            cv.GenerateID(CONF_DISPLAY): cv.use_id(display.DisplayBuffer),
            cv.GenerateID(CONF_DISPLAY_UPDATER): cv.Optional(cv.use_id(cg.PollingComponent)),
            cv.GenerateID(CONF_FONT): cv.use_id(font.Font),
            # cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(lcd_base.LCDDisplay),
            # cv.Optional(CONF_MARK_SELECTED, default=0x3E): cv.uint8_t,
            ##cv.Optional(CONF_MARK_EDITING, default=0x2A): cv.uint8_t,
            # cv.Optional(CONF_MARK_SUBMENU, default=0x7E): cv.uint8_t,
            # cv.Optional(CONF_MARK_BACK, default=0x5E): cv.uint8_t,
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    display = await cg.get_variable(config[CONF_DISPLAY])
    cg.add(var.set_display_buffer(display))

    if CONF_DISPLAY_UPDATER in config:
        display_updater = await cg.get_variable(config[CONF_DISPLAY_UPDATER])
        cg.add(var.set_display_updater(display_updater))

    font = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(font))

    await display_menu_to_code(var, config)
