from esphome import automation, core
import esphome.codegen as cg
from esphome.components import color, display, font
from esphome.components.display_menu_base import (
    DISPLAY_MENU_BASE_SCHEMA,
    DisplayMenuComponent,
    display_menu_to_code,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BACKGROUND_COLOR,
    CONF_DISPLAY,
    CONF_FONT,
    CONF_FOREGROUND_COLOR,
    CONF_ID,
)
from esphome.types import ConfigType

CONF_MENU_ITEM_VALUE = "menu_item_value"
CONF_ON_REDRAW = "on_redraw"

graphical_display_menu_ns = cg.esphome_ns.namespace("graphical_display_menu")
GraphicalDisplayMenu = graphical_display_menu_ns.class_(
    "GraphicalDisplayMenu", DisplayMenuComponent
)
GraphicalDisplayMenuConstPtr = GraphicalDisplayMenu.operator("ptr").operator("const")
MenuItemValueArguments = graphical_display_menu_ns.struct("MenuItemValueArguments")
MenuItemValueArgumentsConstPtr = MenuItemValueArguments.operator("ptr").operator(
    "const"
)

CODEOWNERS = ["@MrMDavidson"]
DOMAIN = "graphical_display_menu"

AUTO_LOAD = ["display_menu_base"]

MULTI_CONF = True

CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GraphicalDisplayMenu),
            cv.Optional(CONF_DISPLAY): cv.use_id(display.Display),
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_MENU_ITEM_VALUE): cv.templatable(cv.string),
            cv.Optional(CONF_FOREGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_BACKGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_ON_REDRAW): automation.validate_automation({}),
        }
    )
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_variable(var.set_display, config.get(CONF_DISPLAY))
    await cg.add_variable(var.set_font, config[CONF_FONT])

    if (menu_item_value_config := config.get(CONF_MENU_ITEM_VALUE, None)) is not None:
        if isinstance(menu_item_value_config, core.Lambda):
            template_ = await cg.templatable(
                menu_item_value_config,
                [(MenuItemValueArgumentsConstPtr, "it")],
                cg.std_string,
            )
            cg.add(var.set_menu_item_value(template_))
        else:
            cg.add(var.set_menu_item_value(menu_item_value_config))

    await cg.add_variable(var.set_foreground_color, config.get(CONF_FOREGROUND_COLOR))
    await cg.add_variable(var.set_background_color, config.get(CONF_BACKGROUND_COLOR))

    for conf in config.get(CONF_ON_REDRAW, []):
        await automation.build_callback_automation(
            var, "add_on_redraw_callback", [(GraphicalDisplayMenuConstPtr, "it")], conf
        )

    await display_menu_to_code(var, config)

    cg.add_define("USE_GRAPHICAL_DISPLAY_MENU")
