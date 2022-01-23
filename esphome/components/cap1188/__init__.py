import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_RESET_PIN
from esphome import pins

CONF_TOUCH_THRESHOLD = "touch_threshold"
CONF_ALLOW_MULTIPLE_TOUCHES = "allow_multiple_touches"

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor", "output"]
CODEOWNERS = ["@MrEditor97"]

cap1188_ns = cg.esphome_ns.namespace("cap1188")
CONF_CAP1188_ID = "cap1188_id"
CAP1188Component = cap1188_ns.class_("CAP1188Component", cg.Component, i2c.I2CDevice)

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CAP1188Component),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_TOUCH_THRESHOLD, default=0x20): cv.int_range(
                min=0x01, max=0x80
            ),
            cv.Optional(CONF_ALLOW_MULTIPLE_TOUCHES, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x29))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_touch_threshold(config[CONF_TOUCH_THRESHOLD]))
    cg.add(var.set_allow_multiple_touches(config[CONF_ALLOW_MULTIPLE_TOUCHES]))

    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
