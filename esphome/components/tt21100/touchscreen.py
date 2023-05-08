import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import binary_sensor, i2c, touchscreen
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN

CODEOWNERS = ["@kroimon"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor"]

tt21100_ns = cg.esphome_ns.namespace("tt21100")
TT21100Touchscreen = tt21100_ns.class_(
    "TT21100Touchscreen",
    touchscreen.Touchscreen,
    cg.Component,
    i2c.I2CDevice,
)

CONF_TT21100_ID = "tt21100_id"
CONF_BUTTON1 = "button1"
CONF_BUTTON2 = "button2"
CONF_BUTTON3 = "button3"
CONF_BUTTON4 = "button4"
CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"

CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TT21100Touchscreen),
            cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUTTON1): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_BUTTON2): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_BUTTON3): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_BUTTON4): binary_sensor.binary_sensor_schema(),
        }
    )
    .extend(i2c.i2c_device_schema(0x24))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(interrupt_pin))
    if CONF_RESET_PIN in config:
        rts_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(rts_pin))

    if CONF_BUTTON1 in config:
        sensor = await binary_sensor.new_binary_sensor(config[CONF_BUTTON1])
        cg.add(var.set_button(0, sensor))
    if CONF_BUTTON2 in config:
        sensor = await binary_sensor.new_binary_sensor(config[CONF_BUTTON2])
        cg.add(var.set_button(1, sensor))
    if CONF_BUTTON3 in config:
        sensor = await binary_sensor.new_binary_sensor(config[CONF_BUTTON3])
        cg.add(var.set_button(2, sensor))
    if CONF_BUTTON4 in config:
        sensor = await binary_sensor.new_binary_sensor(config[CONF_BUTTON4])
        cg.add(var.set_button(3, sensor))
