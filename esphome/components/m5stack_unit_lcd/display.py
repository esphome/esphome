import esphome.codegen as cg
from esphome.components import display, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_BRIGHTNESS,
    CONF_ID,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_ROTATION,
)
from esphome.types import ConfigType

CODEOWNERS = ["@ChillingSilence"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["split_buffer"]

CONF_M5STACK_UNIT_LCD_ID = "m5stack_unit_lcd_id"

# The Unit LCD is a fixed 135x240 panel; there is no model selection.
WIDTH = 135
HEIGHT = 240

m5stack_unit_lcd_ns = cg.esphome_ns.namespace("m5stack_unit_lcd")
M5StackUnitLCD = m5stack_unit_lcd_ns.class_(
    "M5StackUnitLCD", cg.PollingComponent, display.Display, i2c.I2CDevice
)


def _register_metadata(config: ConfigType) -> ConfigType:
    display.add_metadata(
        config[CONF_ID],
        WIDTH,
        HEIGHT,
        has_hardware_rotation=False,
        has_writer=config.get(CONF_AUTO_CLEAR_ENABLED) is True
        or config.get(CONF_PAGES) is not None
        or config.get(CONF_LAMBDA) is not None
        or config.get(display.CONF_SHOW_TEST_CARD) is True,
        rotation=config.get(CONF_ROTATION, 0),
    )
    return config


# The unit's firmware supports I2C clock speeds up to 400 kHz.
FINAL_VALIDATE_SCHEMA = i2c.final_validate_device_schema(
    "m5stack_unit_lcd", max_frequency="400kHz"
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(M5StackUnitLCD),
            cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
            cv.Optional(CONF_INVERT_COLORS, default=False): cv.boolean,
        }
    ).extend(i2c.i2c_device_schema(0x3E)),
    _register_metadata,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))

    if (lambda_config := config.get(CONF_LAMBDA)) is not None:
        lambda_ = await cg.process_lambda(
            lambda_config, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
