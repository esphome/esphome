import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_DIMENSIONS,
)
from esphome.components import lcd_base
from esphome.components.display_menu_base import (
    DISPLAY_MENU_BASE_SCHEMA,
    DisplayMenuComponent,
    display_menu_to_code,
)

CODEOWNERS = ["@numo68"]

AUTO_LOAD = ["display_menu_base"]

lcd_menu_ns = cg.esphome_ns.namespace("lcd_menu")

CONF_DISPLAY_ID = "display_id"

CONF_MARK_SELECTED = "mark_selected"
CONF_MARK_EDITING = "mark_editing"
CONF_MARK_SUBMENU = "mark_submenu"
CONF_MARK_BACK = "mark_back"

LCDCharacterMenuComponent = lcd_menu_ns.class_(
    "LCDCharacterMenuComponent", DisplayMenuComponent
)

MULTI_CONF = True


def validate_lcd_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 64:
        raise cv.Invalid("LCD display can't have more than 64 columns")
    if value[0] < 12:
        raise cv.Invalid(
            "LCD display can't have less than 12 columns to be usable with the menu"
        )
    if value[1] > 4:
        raise cv.Invalid("LCD display can't have more than 4 rows")
    return value


CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LCDCharacterMenuComponent),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(lcd_base.LCDDisplay),
            cv.Required(CONF_DIMENSIONS): validate_lcd_dimensions,
            cv.Optional(CONF_MARK_SELECTED, default=0x3E): cv.uint8_t,
            cv.Optional(CONF_MARK_EDITING, default=0x2A): cv.uint8_t,
            cv.Optional(CONF_MARK_SUBMENU, default=0x7E): cv.uint8_t,
            cv.Optional(CONF_MARK_BACK, default=0x5E): cv.uint8_t,
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(disp))
    cg.add(var.set_dimensions(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1]))
    await display_menu_to_code(var, config)
    cg.add(var.set_mark_selected(config[CONF_MARK_SELECTED]))
    cg.add(var.set_mark_editing(config[CONF_MARK_EDITING]))
    cg.add(var.set_mark_submenu(config[CONF_MARK_SUBMENU]))
    cg.add(var.set_mark_back(config[CONF_MARK_BACK]))
