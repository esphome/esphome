import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
from . import horizontal_stack
from . import vertical_stack
from . import text_panel
from . import display_rendering_panel

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
RootLayoutComponent = graphical_layout_ns.class_("RootLayoutComponent", cg.Component)
LayoutItem = graphical_layout_ns.class_("LayoutItem")
ContainerLayoutItem = graphical_layout_ns.class_("ContainerLayoutItem", LayoutItem)

CODEOWNERS = ["@MrMDavidson"]

AUTO_LOAD = ["display"]

MULTI_CONF = True

CONF_LAYOUT = "layout"

BASE_ITEM_SCHEMA = cv.Schema({})


def item_type_schema(value):
    return ITEM_TYPE_SCHEMA(value)


ITEM_TYPE_SCHEMA = cv.typed_schema(
    {
        text_panel.CONF_TEXT_PANEL: text_panel.get_config_schema(
            BASE_ITEM_SCHEMA, item_type_schema
        ),
        horizontal_stack.CONF_HORIZONTAL_STACK: horizontal_stack.get_config_schema(
            BASE_ITEM_SCHEMA, item_type_schema
        ),
        vertical_stack.CONF_VERTICAL_STACK: vertical_stack.get_config_schema(
            BASE_ITEM_SCHEMA, item_type_schema
        ),
        display_rendering_panel.CONF_DISPLAY_RENDERING_PANEL: display_rendering_panel.get_config_schema(
            BASE_ITEM_SCHEMA, item_type_schema
        ),
    }
)

CODE_GENERATORS = {
    text_panel.CONF_TEXT_PANEL: text_panel.config_to_layout_item,
    horizontal_stack.CONF_HORIZONTAL_STACK: horizontal_stack.config_to_layout_item,
    vertical_stack.CONF_VERTICAL_STACK: vertical_stack.config_to_layout_item,
    display_rendering_panel.CONF_DISPLAY_RENDERING_PANEL: display_rendering_panel.config_to_layout_item,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RootLayoutComponent),
        cv.Required(CONF_LAYOUT): ITEM_TYPE_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    layout_config = config[CONF_LAYOUT]
    layout_type = layout_config[CONF_TYPE]
    if layout_type in CODE_GENERATORS:
        layout_var = await CODE_GENERATORS[layout_type](layout_config, CODE_GENERATORS)
        cg.add(var.set_layout_root(layout_var))
    else:
        raise f"Do not know how to build type {layout_type}"

    cg.add_define("USE_GRAPHICAL_LAYOUT")
