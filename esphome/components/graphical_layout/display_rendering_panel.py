import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.components.display import DisplayRef

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
DisplayRenderingPanel = graphical_layout_ns.class_("DisplayRenderingPanel")

CONF_TYPE = "display_rendering_panel"
CONF_HEIGHT = "height"
CONF_WIDTH = "width"
CONF_LAMBDA = "lambda"

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
