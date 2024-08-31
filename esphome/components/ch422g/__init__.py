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
    CONF_RESTORE_VALUE,
)

CODEOWNERS = ["@jesterret"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
ch422g_ns = cg.esphome_ns.namespace("ch422g")

CH422GComponent = ch422g_ns.class_("CH422GComponent", cg.Component, i2c.I2CDevice)
CH422GGPIOPin = ch422g_ns.class_(
    "CH422GGPIOPin", cg.GPIOPin, cg.Parented.template(CH422GComponent)
)

CONF_CH422G = "ch422g"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(CH422GComponent),
            cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x24))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


CH422G_PIN_SCHEMA = pins.gpio_base_schema(
    CH422GGPIOPin,
    cv.int_range(min=0, max=7),
    modes=[CONF_INPUT, CONF_OUTPUT],
).extend(
    {
        cv.Required(CONF_CH422G): cv.use_id(CH422GComponent),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_CH422G, CH422G_PIN_SCHEMA)
async def ch422g_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_CH422G])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
