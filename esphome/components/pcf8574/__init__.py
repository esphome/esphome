import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_NUMBER,
    CONF_MODE,
    CONF_INVERTED,
    CONF_OUTPUT,
)

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

pcf8574_ns = cg.esphome_ns.namespace("pcf8574")

PCF8574Component = pcf8574_ns.class_("PCF8574Component", cg.Component, i2c.I2CDevice)
PCF8574GPIOPin = pcf8574_ns.class_("PCF8574GPIOPin", cg.GPIOPin)

CONF_PCF8574 = "pcf8574"
CONF_PCF8575 = "pcf8575"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCF8574Component),
            cv.Optional(CONF_PCF8575, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x21))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_pcf8575(config[CONF_PCF8575]))


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


PCF8574_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(PCF8574GPIOPin),
        cv.Required(CONF_PCF8574): cv.use_id(PCF8574Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=17),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
            validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register("pcf8574", PCF8574_PIN_SCHEMA)
async def pcf8574_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_PCF8574])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
