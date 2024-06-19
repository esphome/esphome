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
    CONF_PULLUP,
)

CODEOWNERS = ["@Mat931"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
pca6416a_ns = cg.esphome_ns.namespace("pca6416a")

PCA6416AComponent = pca6416a_ns.class_("PCA6416AComponent", cg.Component, i2c.I2CDevice)
PCA6416AGPIOPin = pca6416a_ns.class_(
    "PCA6416AGPIOPin", cg.GPIOPin, cg.Parented.template(PCA6416AComponent)
)

CONF_PCA6416A = "pca6416a"
CONFIG_SCHEMA = (
    cv.Schema({cv.Required(CONF_ID): cv.declare_id(PCA6416AComponent)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x21))
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


PCA6416A_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(PCA6416AGPIOPin),
        cv.Required(CONF_PCA6416A): cv.use_id(PCA6416AComponent),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=16),
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


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PCA6416A, PCA6416A_PIN_SCHEMA)
async def pca6416a_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_PCA6416A])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
