import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins, automation
from esphome.components import i2c
from esphome.const import CONF_HEIGHT, CONF_ID, CONF_ROTATION, CONF_WIDTH

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]

ektf2232_ns = cg.esphome_ns.namespace("ektf2232")
EKTF2232Touchscreen = ektf2232_ns.class_(
    "EKTF2232Touchscreen", cg.Component, i2c.I2CDevice
)
TouchPoint = ektf2232_ns.struct("TouchPoint")
TouchListener = ektf2232_ns.class_("TouchListener")

EKTF2232Rotation = ektf2232_ns.enum("EKTF2232Rotation")

CONF_EKTF2232_ID = "ektf2232_id"
CONF_INTERRUPT_PIN = "interrupt_pin"
CONF_RTS_PIN = "rts_pin"
CONF_ON_TOUCH = "on_touch"

ROTATIONS = {
    0: EKTF2232Rotation.ROTATE_0_DEGREES,
    90: EKTF2232Rotation.ROTATE_90_DEGREES,
    180: EKTF2232Rotation.ROTATE_180_DEGREES,
    270: EKTF2232Rotation.ROTATE_270_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith("°"):
        value = value[:-1]
    return cv.enum(ROTATIONS, int=True)(value)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EKTF2232Touchscreen),
            cv.Required(CONF_INTERRUPT_PIN): cv.All(
                pins.internal_gpio_input_pin_schema
            ),
            cv.Required(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_HEIGHT, default=758): cv.int_,
            cv.Optional(CONF_WIDTH, default=1024): cv.int_,
            cv.Optional(CONF_ROTATION, default=0): validate_rotation,
            cv.Optional(CONF_ON_TOUCH): automation.validate_automation(single=True),
        }
    )
    .extend(i2c.i2c_device_schema(0x15))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(interrupt_pin))
    rts_pin = await cg.gpio_pin_expression(config[CONF_RTS_PIN])
    cg.add(var.set_rts_pin(rts_pin))

    cg.add(
        var.set_display_details(
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            config[CONF_ROTATION],
        )
    )

    if CONF_ON_TOUCH in config:
        await automation.build_automation(
            var.get_touch_trigger(), [(TouchPoint, "touch")], config[CONF_ON_TOUCH]
        )
