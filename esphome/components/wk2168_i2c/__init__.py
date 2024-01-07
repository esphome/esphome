import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c, wk2168
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["wk2168"]
MULTI_CONF = True
CONF_WK2168 = "wk2168_i2c"

wk2168_ns = cg.esphome_ns.namespace("wk2168_i2c")
WK2168ComponentI2C = wk2168_ns.class_(
    "WK2168ComponentI2C", wk2168.WK2168Component, i2c.I2CDevice
)
WK2168GPIOPinI2C = wk2168_ns.class_(
    "WK2168GPIOPinI2C", cg.GPIOPin, cg.Parented.template(WK2168ComponentI2C)
)

CONFIG_SCHEMA = cv.All(
    wk2168.WK2168_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2168ComponentI2C),
        }
    ).extend(i2c.i2c_device_schema(0x2C)),
    wk2168.post_check_conf_wk2168,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add_define("I2C_COMPILE")  # add to defines.h
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk2168.register_wk2168(var, config)
    await i2c.register_i2c_device(var, config)


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


WK2168_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(WK2168GPIOPinI2C),
        cv.Required(CONF_WK2168): cv.use_id(WK2168ComponentI2C),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=8),
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


@pins.PIN_SCHEMA_REGISTRY.register("wk2168_i2c", WK2168_PIN_SCHEMA)
async def sc16is75x_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_WK2168])
    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
