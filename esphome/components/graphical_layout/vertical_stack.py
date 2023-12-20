import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
VerticalStack = graphical_layout_ns.class_("VerticalStack")

CONF_ITEM_PADDING = "item_padding"
CONF_VERTICAL_STACK = "vertical_stack"
CONF_ITEMS = "items"


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(VerticalStack),
            cv.Optional(CONF_ITEM_PADDING, default=0): cv.templatable(cv.int_),
            cv.Required(CONF_ITEMS): cv.All(
                cv.ensure_list(item_type_schema), cv.Length(min=1)
            ),
        }
    )


async def config_to_layout_item(item_config, child_item_builder):
    var = cg.new_Pvariable(item_config[CONF_ID])

    if item_padding_config := item_config[CONF_ITEM_PADDING]:
        cg.add(var.set_item_padding(item_padding_config))

    for child_item_config in item_config[CONF_ITEMS]:
        child_item_type = child_item_config[CONF_TYPE]
        if child_item_type in child_item_builder:
            child_item_var = await child_item_builder[child_item_type](
                child_item_config, child_item_builder
            )
            cg.add(var.add_item(child_item_var))
        else:
            raise f"Do not know how to build type {child_item_type}"

    return var
