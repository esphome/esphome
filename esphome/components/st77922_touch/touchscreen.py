from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN

# st77922_touch — touchscreen platform for the Sitronix-style touch controller
# on the Freenove FNK0104N (I2C 0x55).

DEPENDENCIES = ["i2c"]

st77922_touch_ns = cg.esphome_ns.namespace("st77922_touch")
ST77922Touchscreen = st77922_touch_ns.class_(
    "ST77922Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = (
    touchscreen.touchscreen_schema()
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ST77922Touchscreen),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x55))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_INTERRUPT_PIN in config:
        interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_interrupt_pin(interrupt_pin))
    if CONF_RESET_PIN in config:
        reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset_pin))
