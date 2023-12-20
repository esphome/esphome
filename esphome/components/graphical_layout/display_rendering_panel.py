import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WIDTH, CONF_HEIGHT, CONF_LAMBDA
from esphome.components.display import DisplayRef

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
DisplayRenderingPanel = graphical_layout_ns.class_("DisplayRenderingPanel")

CONF_DISPLAY_RENDERING_PANEL = "display_rendering_panel"


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(DisplayRenderingPanel),
            cv.Required(CONF_WIDTH): cv.templatable(cv.int_range(min=1)),
            cv.Required(CONF_HEIGHT): cv.templatable(cv.int_range(min=1)),
            cv.Required(CONF_LAMBDA): cv.lambda_,
        }
    )


async def config_to_layout_item(item_config, child_item_builder):
    var = cg.new_Pvariable(item_config[CONF_ID])

    width = await cg.templatable(item_config[CONF_WIDTH], args=[], output_type=int)
    cg.add(var.set_width(width))

    height = await cg.templatable(item_config[CONF_HEIGHT], args=[], output_type=int)
    cg.add(var.set_height(height))

    lambda_ = await cg.process_lambda(
        item_config[CONF_LAMBDA], [(DisplayRef, "it")], return_type=cg.void
    )
    cg.add(var.set_lambda(lambda_))

    return var
