import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID
from esphome.types import ConfigType

from ..display import CONF_M5STACK_UNIT_LCD_ID, M5StackUnitLCD, m5stack_unit_lcd_ns

M5StackUnitLCDLight = m5stack_unit_lcd_ns.class_(
    "M5StackUnitLCDLight", light.LightOutput
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5StackUnitLCDLight),
        cv.GenerateID(CONF_M5STACK_UNIT_LCD_ID): cv.use_id(M5StackUnitLCD),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_M5STACK_UNIT_LCD_ID])
