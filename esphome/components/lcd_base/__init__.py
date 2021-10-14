import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import CONF_DIMENSIONS

lcd_base_ns = cg.esphome_ns.namespace("lcd_base")
LCDDisplay = lcd_base_ns.class_("LCDDisplay", cg.PollingComponent)


def validate_lcd_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 0x40:
        raise cv.Invalid("LCD displays can't have more than 64 columns")
    if value[1] > 4:
        raise cv.Invalid("LCD displays can't have more than 4 rows")
    return value


LCD_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Required(CONF_DIMENSIONS): validate_lcd_dimensions,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_lcd_display(var, config):
    await cg.register_component(var, config)
    await display.register_display(var, config)
    cg.add(var.set_dimensions(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1]))
