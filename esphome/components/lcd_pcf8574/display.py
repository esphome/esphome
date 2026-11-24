import esphome.codegen as cg
from esphome.components import display, i2c, lcd_base
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["lcd_base"]

lcd_pcf8574_ns = cg.esphome_ns.namespace("lcd_pcf8574")
PCF8574LCDDisplay = lcd_pcf8574_ns.class_(
    "PCF8574LCDDisplay", lcd_base.LCDDisplay, i2c.I2CDevice
)

CONFIG_SCHEMA = lcd_base.LCD_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PCF8574LCDDisplay),
    }
).extend(i2c.i2c_device_schema(0x3F))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await lcd_base.setup_lcd_display(var, config)
    await i2c.register_i2c_device(var, config)

    await display.add_writer(var, config, PCF8574LCDDisplay.operator("ref"))
