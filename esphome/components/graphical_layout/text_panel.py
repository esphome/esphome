import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, color
from esphome.const import CONF_ID

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
TextPanel = graphical_layout_ns.class_("TextPanel")

CONF_ITEM_PADDING = "item_padding"
CONF_TEXT_PANEL = "text_panel"
CONF_FONT = "font"
CONF_FOREGROUND_COLOR = "foreground_color"
CONF_BACKGROUND_COLOR = "background_color"
CONF_TEXT = "text"


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(TextPanel),
            cv.Optional(CONF_ITEM_PADDING, default=0): cv.templatable(cv.int_),
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_FOREGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_BACKGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Required(CONF_TEXT): cv.templatable(cv.string),
        }
    )


async def config_to_layout_item(item_config, child_item_builder):
    var = cg.new_Pvariable(item_config[CONF_ID])

    if item_padding_config := item_config[CONF_ITEM_PADDING]:
        cg.add(var.set_item_padding(item_padding_config))

    panel_font = await cg.get_variable(item_config[CONF_FONT])
    cg.add(var.set_font(panel_font))

    if foreground_color_config := item_config.get(CONF_FOREGROUND_COLOR):
        foreground_color = await cg.get_variable(foreground_color_config)
        cg.add(var.set_foreground_color(foreground_color))

    if background_color_config := item_config.get(CONF_BACKGROUND_COLOR):
        background_color = await cg.get_variable(background_color_config)
        cg.add(var.set_background_color(background_color))

    text = await cg.templatable(item_config[CONF_TEXT], args=[], output_type=str)
    cg.add(var.set_text(text))

    return var
