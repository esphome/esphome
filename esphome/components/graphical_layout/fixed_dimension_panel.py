import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, CONF_WIDTH, CONF_HEIGHT

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
FixedDimensionPanel = graphical_layout_ns.class_("FixedDimensionPanel")

CONF_ITEM_PADDING = "item_padding"
CONF_FIXED_DIMENSION_PANEL = "fixed_dimension_panel"
CONF_CHILD = "child"


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(FixedDimensionPanel),
            cv.Optional(CONF_ITEM_PADDING, default=0): cv.int_,
            cv.Required(CONF_CHILD): item_type_schema,
            cv.Optional(CONF_WIDTH, default=-1): cv.templatable(cv.int_range(min=-1)),
            cv.Optional(CONF_HEIGHT, default=-1): cv.templatable(cv.int_range(min=-1)),
        }
    )


async def config_to_layout_item(pvariable_builder, item_config, child_item_builder):
    var = await pvariable_builder(item_config)

    width = await cg.templatable(item_config[CONF_WIDTH], args=[], output_type=cg.int_)
    cg.add(var.set_width(width))

    height = await cg.templatable(
        item_config[CONF_HEIGHT], args=[], output_type=cg.int_
    )
    cg.add(var.set_height(height))

    child_item_config = item_config[CONF_CHILD]
    child_item_type = child_item_config[CONF_TYPE]
    if child_item_type in child_item_builder:
        child_item_var = await child_item_builder[child_item_type](
            pvariable_builder, child_item_config, child_item_builder
        )
        cg.add(var.set_child(child_item_var))
    else:
        raise f"Do not know how to build type {child_item_type}"

    return var
