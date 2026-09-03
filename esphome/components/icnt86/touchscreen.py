from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN
from esphome.types import ConfigType

CODEOWNERS = ["@siemon-geeroms"]
DEPENDENCIES = ["i2c"]

icnt86_ns = cg.esphome_ns.namespace("icnt86")
ICNT86Touchscreen = icnt86_ns.class_(
    "ICNT86Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = touchscreen.touchscreen_schema("250ms").extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ICNT86Touchscreen),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    ).extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    if interrupt_pin_config := config.get(CONF_INTERRUPT_PIN):
        cg.add(
            var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin_config))
        )

    if reset_pin_config := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin_config)))
