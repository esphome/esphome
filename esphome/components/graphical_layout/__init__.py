import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, color
from esphome.const import CONF_ID
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

CONF_ITEMS = "items"
CONF_LAYOUT = "layout"
CONF_ITEM_TYPE = "type"

BASE_ITEM_SCHEMA = cv.Schema({})


def item_type_schema(value):
    return ITEM_TYPE_SCHEMA(value)


ITEM_TYPE_SCHEMA = cv.typed_schema(
    {
        text_panel.CONF_TYPE: BASE_ITEM_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(text_panel.TextPanel),
                cv.Optional(text_panel.CONF_ITEM_PADDING, default=0): cv.templatable(
                    cv.int_
                ),
                cv.Required(text_panel.CONF_FONT): cv.use_id(font.Font),
                cv.Optional(text_panel.CONF_FOREGROUND_COLOR): cv.use_id(
                    color.ColorStruct
                ),
                cv.Optional(text_panel.CONF_BACKGROUND_COLOR): cv.use_id(
                    color.ColorStruct
                ),
                cv.Required(text_panel.CONF_TEXT): cv.templatable(cv.string),
            }
        ),
        horizontal_stack.CONF_TYPE: BASE_ITEM_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(horizontal_stack.HorizontalStack),
                cv.Optional(
                    horizontal_stack.CONF_ITEM_PADDING, default=0
                ): cv.templatable(cv.int_),
                cv.Required(CONF_ITEMS): cv.All(
                    cv.ensure_list(item_type_schema), cv.Length(min=1)
                ),
            }
        ),
        vertical_stack.CONF_TYPE: BASE_ITEM_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(vertical_stack.VerticalStack),
                cv.Optional(
                    vertical_stack.CONF_ITEM_PADDING, default=0
                ): cv.templatable(cv.int_),
                cv.Required(CONF_ITEMS): cv.All(
                    cv.ensure_list(item_type_schema), cv.Length(min=1)
                ),
            }
        ),
        display_rendering_panel.CONF_TYPE: BASE_ITEM_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(
                    display_rendering_panel.DisplayRenderingPanel
                ),
                cv.Required(display_rendering_panel.CONF_WIDTH): cv.templatable(
                    cv.int_range(min=1)
                ),
                cv.Required(display_rendering_panel.CONF_HEIGHT): cv.templatable(
                    cv.int_range(min=1)
                ),
                cv.Required(display_rendering_panel.CONF_LAMBDA): cv.lambda_,
            }
        ),
    }
)

CODE_GENERATORS = {
    text_panel.CONF_TYPE: text_panel.config_to_layout_item,
    horizontal_stack.CONF_TYPE: horizontal_stack.config_to_layout_item,
    vertical_stack.CONF_TYPE: vertical_stack.config_to_layout_item,
    display_rendering_panel.CONF_TYPE: display_rendering_panel.config_to_layout_item,
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
    layout_type = layout_config[CONF_ITEM_TYPE]
    if layout_type in CODE_GENERATORS:
        layout_var = await CODE_GENERATORS[layout_type](layout_config, CODE_GENERATORS)
        cg.add(var.set_layout_root(layout_var))
    else:
        raise f"Do not know how to build type {layout_type}"

    cg.add_define("USE_GRAPHICAL_LAYOUT")
