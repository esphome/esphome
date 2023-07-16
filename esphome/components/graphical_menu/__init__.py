import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_COLOR,
)
from esphome.components import display, font, color
from esphome.components.display_menu_base import (
    DISPLAY_MENU_BASE_SCHEMA,
    DisplayMenuComponent,
    display_menu_to_code,
)

CODEOWNERS = ["@silverchris"]

AUTO_LOAD = ["display_menu_base"]

graphical_menu_ns = cg.esphome_ns.namespace("graphical_menu")

CONF_DISPLAY_ID = "display_id"

CONF_MARK_SELECTED = "mark_selected"
CONF_MARK_EDITING = "mark_editing"
CONF_MARK_SUBMENU = "mark_submenu"
CONF_MARK_BACK = "mark_back"
CONF_FONT = "font"

MINIMUM_COLUMNS = 12

GraphicalMenuComponent = graphical_menu_ns.class_(
    "GraphicalMenuComponent", DisplayMenuComponent
)

MULTI_CONF = True

CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GraphicalMenuComponent),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.DisplayBuffer),
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_MARK_SELECTED, default=0x3E): cv.uint8_t,
            cv.Optional(CONF_MARK_EDITING, default=0x2A): cv.uint8_t,
            cv.Optional(CONF_MARK_SUBMENU, default=0x7E): cv.uint8_t,
            cv.Optional(CONF_MARK_BACK, default=0x5E): cv.uint8_t,
            cv.Optional(CONF_COLOR): cv.use_id(color.ColorStruct),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(disp))
    await display_menu_to_code(var, config)
    cg.add(var.set_mark_selected(config[CONF_MARK_SELECTED]))
    cg.add(var.set_mark_editing(config[CONF_MARK_EDITING]))
    cg.add(var.set_mark_submenu(config[CONF_MARK_SUBMENU]))
    cg.add(var.set_mark_back(config[CONF_MARK_BACK]))
    menu_font = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(menu_font))
    if CONF_COLOR in config:
        c = await cg.get_variable(config[CONF_COLOR])
        cg.add(var.set_color(c))
