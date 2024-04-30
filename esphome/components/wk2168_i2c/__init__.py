import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c, weikai
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
)

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["weikai", "weikai_i2c"]
MULTI_CONF = True
CONF_WK2168_I2C = "wk2168_i2c"

weikai_ns = cg.esphome_ns.namespace("weikai")
weikai_i2c_ns = cg.esphome_ns.namespace("weikai_i2c")
WeikaiComponentI2C = weikai_i2c_ns.class_(
    "WeikaiComponentI2C", weikai.WeikaiComponent, i2c.I2CDevice
)
WeikaiGPIOPin = weikai_ns.class_(
    "WeikaiGPIOPin", cg.GPIOPin, cg.Parented.template(WeikaiComponentI2C)
)

CONFIG_SCHEMA = cv.All(
    weikai.WKBASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WeikaiComponentI2C),
        }
    ).extend(i2c.i2c_device_schema(0x2C)),
    weikai.check_channel_max_4,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_name(str(config[CONF_ID])))
    await weikai.register_weikai(var, config)
    await i2c.register_i2c_device(var, config)


WK2168_PIN_SCHEMA = cv.All(
    weikai.WEIKAI_PIN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WeikaiGPIOPin),
            cv.Required(CONF_WK2168_I2C): cv.use_id(WeikaiComponentI2C),
        }
    ),
    weikai.validate_pin_mode,
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_WK2168_I2C, WK2168_PIN_SCHEMA)
async def sc16is75x_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_WK2168_I2C])
    cg.add(var.set_parent(parent))
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
