import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
HorizontalStack = graphical_layout_ns.class_("HorizontalStack")
VerticalChildAlign = graphical_layout_ns.enum("VerticalChildAlign", is_class=True)

CONF_ITEM_PADDING = "item_padding"
CONF_HORIZONTAL_STACK = "horizontal_stack"
CONF_ITEMS = "items"
CONF_CHILD_ALIGN = "child_align"

VERTICAL_CHILD_ALIGN = {
    "TOP": VerticalChildAlign.TOP,
    "CENTER_VERTICAL": VerticalChildAlign.CENTER_VERTICAL,
    "BOTTOM": VerticalChildAlign.BOTTOM,
    "STRETCH_TO_FIT_HEIGHT": VerticalChildAlign.STRETCH_TO_FIT_HEIGHT,
}


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(HorizontalStack),
            cv.Optional(CONF_ITEM_PADDING, default=0): cv.templatable(cv.int_),
            cv.Required(CONF_ITEMS): cv.All(
                cv.ensure_list(item_type_schema), cv.Length(min=1)
            ),
            cv.Optional(CONF_CHILD_ALIGN): cv.enum(VERTICAL_CHILD_ALIGN, upper=True),
        }
    )


async def config_to_layout_item(pvariable_builder, item_config, child_item_builder):
    var = await pvariable_builder(item_config)

    if item_padding_config := item_config.get(CONF_ITEM_PADDING):
        cg.add(var.set_item_padding(item_padding_config))

    if child_align := item_config.get(CONF_CHILD_ALIGN):
        cg.add(var.set_child_align(child_align))

    for child_item_config in item_config.get(CONF_ITEMS):
        child_item_type = child_item_config[CONF_TYPE]
        if child_item_type in child_item_builder:
            child_item_var = await child_item_builder[child_item_type](
                pvariable_builder, child_item_config, child_item_builder
            )
            cg.add(var.add_item(child_item_var))
        else:
            raise f"Do not know how to build type {child_item_type}"

    return var
