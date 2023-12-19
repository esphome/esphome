import esphome.codegen as cg
from esphome.const import CONF_ID

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
VerticalStack = graphical_layout_ns.class_("VerticalStack")

CONF_ITEM_PADDING = "item_padding"
CONF_TYPE = "vertical_stack"
CONF_ITEMS = "items"
CONF_TYPE_KEY = "type"


async def config_to_layout_item(item_config, child_item_builder):
    var = cg.new_Pvariable(item_config[CONF_ID])

    if item_padding_config := item_config[CONF_ITEM_PADDING]:
        cg.add(var.set_item_padding(item_padding_config))

    for child_item_config in item_config[CONF_ITEMS]:
        child_item_type = child_item_config[CONF_TYPE_KEY]
        if child_item_type in child_item_builder:
            child_item_var = await child_item_builder[child_item_type](
                child_item_config, child_item_builder
            )
            cg.add(var.add_item(child_item_var))
        else:
            raise f"Do not know how to build type {child_item_type}"

    return var
