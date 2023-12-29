import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, color
from esphome.components.display import display_ns
# from esphome.const import CONF_FOREGROUND_COLOR, CONF_BACKGROUND_COLOR

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
TextPanel = graphical_layout_ns.class_("TextPanel")
TextAlign = display_ns.enum("TextAlign", is_class=True)

CONF_TEXT_PANEL = "text_panel"
CONF_FONT = "font"
CONF_TEXT = "text"
CONF_TEXT_ALIGN = "text_align"


TEXT_ALIGN = {
    "TOP_LEFT": TextAlign.TOP_LEFT,
    "TOP_CENTER": TextAlign.TOP_CENTER,
    "TOP_RIGHT": TextAlign.TOP_RIGHT,
    "CENTER_LEFT": TextAlign.CENTER_LEFT,
    "CENTER": TextAlign.CENTER,
    "CENTER_RIGHT": TextAlign.CENTER_RIGHT,
    "BASELINE_LEFT": TextAlign.BASELINE_LEFT,
    "BASELINE_CENTER": TextAlign.BASELINE_CENTER,
    "BASELINE_RIGHT": TextAlign.BASELINE_RIGHT,
    "BOTTOM_LEFT": TextAlign.BOTTOM_LEFT,
    "BOTTOM_CENTER": TextAlign.BOTTOM_CENTER,
    "BOTTOM_RIGHT": TextAlign.BOTTOM_RIGHT,
}


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(TextPanel),
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_FOREGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Optional(CONF_BACKGROUND_COLOR): cv.use_id(color.ColorStruct),
            cv.Required(CONF_TEXT): cv.templatable(cv.string),
            cv.Optional(CONF_TEXT_ALIGN): cv.enum(TEXT_ALIGN, upper=True),
        }
    )


async def config_to_layout_item(pvariable_builder, item_config, child_item_builder):
    var = await pvariable_builder(item_config)

    panel_font = await cg.get_variable(item_config[CONF_FONT])
    cg.add(var.set_font(panel_font))

    if foreground_color_config := item_config.get(CONF_FOREGROUND_COLOR):
        foreground_color = await cg.get_variable(foreground_color_config)
        cg.add(var.set_foreground_color(foreground_color))

    if background_color_config := item_config.get(CONF_BACKGROUND_COLOR):
        background_color = await cg.get_variable(background_color_config)
        cg.add(var.set_background_color(background_color))

    text = await cg.templatable(
        item_config[CONF_TEXT], args=[], output_type=cg.std_string
    )
    cg.add(var.set_text(text))

    if text_align := item_config.get(CONF_TEXT_ALIGN):
        cg.add(var.set_text_align(text_align))

    return var
