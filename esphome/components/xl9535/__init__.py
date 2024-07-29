import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)
from esphome import pins

CONF_XL9535 = "xl9535"

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@mreditor97"]

xl9535_ns = cg.esphome_ns.namespace(CONF_XL9535)

XL9535Component = xl9535_ns.class_("XL9535Component", cg.Component, i2c.I2CDevice)
XL9535GPIOPin = xl9535_ns.class_("XL9535GPIOPin", cg.GPIOPin)

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema({cv.Required(CONF_ID): cv.declare_id(XL9535Component)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


def validate_mode(mode):
    if not (mode[CONF_INPUT] or mode[CONF_OUTPUT]) or (
        mode[CONF_INPUT] and mode[CONF_OUTPUT]
    ):
        raise cv.Invalid("Mode must be either a input or a output")
    return mode


def validate_pin(pin):
    if pin in (8, 9):
        raise cv.Invalid(f"pin {pin} doesn't exist")
    return pin


XL9535_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(XL9535GPIOPin),
        cv.Required(CONF_XL9535): cv.use_id(XL9535Component),
        cv.Required(CONF_NUMBER): cv.All(cv.int_range(min=0, max=17), validate_pin),
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


@pins.PIN_SCHEMA_REGISTRY.register(CONF_XL9535, XL9535_PIN_SCHEMA)
async def xl9535_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_XL9535])

    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))

    return var
