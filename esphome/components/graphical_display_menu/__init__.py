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
    CONF_TRIGGER_ID,
)

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
GraphicalDisplayMenuOnRedrawTrigger = graphical_display_menu_ns.class_(
    "GraphicalDisplayMenuOnRedrawTrigger", automation.Trigger
)

CODEOWNERS = ["@MrMDavidson"]

AUTO_LOAD = ["display_menu_base"]

CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GraphicalDisplayMenu),
            cv.Optional(CONF_DISPLAY): cv.use_id(display.Display),
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_MENU_ITEM_VALUE): cv.templatable(cv.string),
            cv.Optional(CONF_FOREGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_BACKGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_ON_REDRAW): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        GraphicalDisplayMenuOnRedrawTrigger
                    )
                }
            ),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if display_config := config.get(CONF_DISPLAY):
        drawing_display = await cg.get_variable(display_config)
        cg.add(var.set_display(drawing_display))

    menu_font = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(menu_font))

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

    if foreground_color_config := config.get(CONF_FOREGROUND_COLOR):
        foreground_color = await cg.get_variable(foreground_color_config)
        cg.add(var.set_foreground_color(foreground_color))

    if background_color_config := config.get(CONF_BACKGROUND_COLOR):
        background_color = await cg.get_variable(background_color_config)
        cg.add(var.set_background_color(background_color))

    for conf in config.get(CONF_ON_REDRAW, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(GraphicalDisplayMenuConstPtr, "it")], conf
        )

    await display_menu_to_code(var, config)

    cg.add_define("USE_GRAPHICAL_DISPLAY_MENU")
