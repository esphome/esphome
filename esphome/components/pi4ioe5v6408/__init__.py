from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_RESET,
)

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True


pi4ioe5v6408_ns = cg.esphome_ns.namespace("pi4ioe5v6408")
PI4IOE5V6408Component = pi4ioe5v6408_ns.class_(
    "PI4IOE5V6408Component", cg.Component, i2c.I2CDevice
)
PI4IOE5V6408GPIOPin = pi4ioe5v6408_ns.class_("PI4IOE5V6408GPIOPin", cg.GPIOPin)

CONF_PI4IOE5V6408 = "pi4ioe5v6408"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PI4IOE5V6408Component),
            cv.Optional(CONF_RESET, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x43))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_reset(config[CONF_RESET]))


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


PI4IOE5V6408_PIN_SCHEMA = pins.gpio_base_schema(
    PI4IOE5V6408GPIOPin,
    cv.int_range(min=0, max=7),
    modes=[
        CONF_INPUT,
        CONF_OUTPUT,
        CONF_PULLUP,
        CONF_PULLDOWN,
    ],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_PI4IOE5V6408): cv.use_id(PI4IOE5V6408Component),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PI4IOE5V6408, PI4IOE5V6408_PIN_SCHEMA)
async def pi4ioe5v6408_pin_schema(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_PI4IOE5V6408])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
