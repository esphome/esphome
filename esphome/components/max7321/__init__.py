import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_NUMBER,
    CONF_MODE,
    CONF_INVERTED,
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_PULLUP,
)

# code heavily inspired by looping40
CODEOWNERS = ["@sqozz"]

DEPENDENCIES = ["i2c"]
MULTI_CONF = True


max7321_ns = cg.esphome_ns.namespace("max7321")

MAX7321 = max7321_ns.class_("MAX7321", cg.Component, i2c.I2CDevice)
MAX7321GPIOPin = max7321_ns.class_("MAX7321GPIOPin", cg.GPIOPin)

# Actions
SetCurrentGlobalAction = max7321_ns.class_("SetCurrentGlobalAction", automation.Action)
SetCurrentModeAction = max7321_ns.class_("SetCurrentModeAction", automation.Action)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(MAX7321),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLUP] and not value[CONF_INPUT]:
        raise cv.Invalid("Pullup only available with input")
    return value


CONF_MAX7321 = "max7321"

MAX7321_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(MAX7321GPIOPin),
        cv.Required(CONF_MAX7321): cv.use_id(MAX7321),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=7),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_PULLUP, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
            validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MAX7321, MAX7321_PIN_SCHEMA)
async def max7321_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_MAX7321])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
