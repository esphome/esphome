import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_BORDER
from esphome.components import color
from . import horizontal_stack
from . import vertical_stack
from . import text_panel
from . import display_rendering_panel
from . import fixed_dimension_panel
from . import text_run_panel

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
RootLayoutComponent = graphical_layout_ns.class_("RootLayoutComponent", cg.Component)
LayoutItem = graphical_layout_ns.class_("LayoutItem")
ContainerLayoutItem = graphical_layout_ns.class_("ContainerLayoutItem", LayoutItem)

CODEOWNERS = ["@MrMDavidson"]

AUTO_LOAD = ["display"]

MULTI_CONF = True

CONF_LAYOUT = "layout"
CONF_MARGIN = "margin"
CONF_PADDING = "padding"
CONF_BORDER_COLOR = "border_color"
CONF_LEFT = "left"
CONF_TOP = "top"
CONF_RIGHT = "right"
CONF_BOTTOM = "bottom"

DIMENSION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LEFT, default=0): cv.int_range(min=0),
        cv.Optional(CONF_TOP, default=0): cv.int_range(min=0),
        cv.Optional(CONF_RIGHT, default=0): cv.int_range(min=0),
        cv.Optional(CONF_BOTTOM, default=0): cv.int_range(min=0),
    }
)

BASE_ITEM_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MARGIN, default=0): cv.Any(
            DIMENSION_SCHEMA, cv.int_range(min=0)
        ),
        cv.Optional(CONF_BORDER, default=0): cv.Any(
            DIMENSION_SCHEMA, cv.int_range(min=0)
        ),
        cv.Optional(CONF_BORDER_COLOR): cv.use_id(color.ColorStruct),
        cv.Optional(CONF_PADDING, default=0): cv.Any(
            DIMENSION_SCHEMA, cv.int_range(min=0)
        ),
    }
)


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
        fixed_dimension_panel.CONF_FIXED_DIMENSION_PANEL: fixed_dimension_panel.get_config_schema(
            BASE_ITEM_SCHEMA, item_type_schema
        ),
        text_run_panel.CONF_TEXT_RUN_PANEL: text_run_panel.get_config_schema(
            BASE_ITEM_SCHEMA, item_type_schema
        ),
    }
)

CODE_GENERATORS = {
    text_panel.CONF_TEXT_PANEL: text_panel.config_to_layout_item,
    horizontal_stack.CONF_HORIZONTAL_STACK: horizontal_stack.config_to_layout_item,
    vertical_stack.CONF_VERTICAL_STACK: vertical_stack.config_to_layout_item,
    display_rendering_panel.CONF_DISPLAY_RENDERING_PANEL: display_rendering_panel.config_to_layout_item,
    fixed_dimension_panel.CONF_FIXED_DIMENSION_PANEL: fixed_dimension_panel.config_to_layout_item,
    text_run_panel.CONF_TEXT_RUN_PANEL: text_run_panel.config_to_layout_item,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RootLayoutComponent),
        cv.Required(CONF_LAYOUT): ITEM_TYPE_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def extract_dimension_expression(value_config, individual_set, single_set):
    if value_config is not None:
        if not isinstance(value_config, int):
            # Handle individual dimensions
            left = value_config.get(CONF_LEFT)
            top = value_config.get(CONF_TOP)
            right = value_config.get(CONF_RIGHT)
            bottom = value_config.get(CONF_BOTTOM)

            individual_set(left, top, right, bottom)
        else:
            template = await cg.templatable(value_config, args=[], output_type=int)
            single_set(template)


async def build_layout_item_pvariable(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await extract_dimension_expression(
        config.get(CONF_MARGIN),
        lambda left, top, right, bottom: cg.add(
            var.set_margin(left, top, right, bottom)
        ),
        lambda margin: cg.add(var.set_margin(margin)),
    )

    await extract_dimension_expression(
        config.get(CONF_BORDER),
        lambda left, top, right, bottom: cg.add(
            var.set_border(left, top, right, bottom)
        ),
        lambda border: cg.add(var.set_border(border)),
    )

    await extract_dimension_expression(
        config.get(CONF_PADDING),
        lambda left, top, right, bottom: cg.add(
            var.set_padding(left, top, right, bottom)
        ),
        lambda padding: cg.add(var.set_padding(padding)),
    )

    if border_color_config := config.get(CONF_BORDER_COLOR):
        border_color = await cg.get_variable(border_color_config)
        cg.add(var.set_border_color(border_color))

    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    layout_config = config[CONF_LAYOUT]
    layout_type = layout_config[CONF_TYPE]
    if layout_type in CODE_GENERATORS:
        layout_var = await CODE_GENERATORS[layout_type](
            build_layout_item_pvariable, layout_config, CODE_GENERATORS
        )
        cg.add(var.set_layout_root(layout_var))
    else:
        raise RuntimeError(f"Do not know how to build type {layout_type}")

    cg.add_define("USE_GRAPHICAL_LAYOUT")
