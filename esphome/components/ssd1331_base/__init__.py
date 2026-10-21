from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_RESET_PIN
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DOMAIN = "ssd1331_base"

ssd1331_base_ns = cg.esphome_ns.namespace("ssd1331_base")
SSD1331 = ssd1331_base_ns.class_("SSD1331", cg.PollingComponent, display.DisplayBuffer)

SSD1331_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_ssd1331(var: MockObj, config: ConfigType) -> None:
    await display.register_display(var, config)

    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
    cg.add_optional(var.init_brightness, config.get(CONF_BRIGHTNESS))
    await display.add_writer(var, config)
