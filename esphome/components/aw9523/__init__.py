from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INVERTED, CONF_MODE, CONF_NUMBER

CODEOWNERS = ["@beormund"]
CONF_AW9523 = "aw9523"
CONF_DIV = "divider"
CONF_LATCH_INPUTS = "latch_inputs"
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

aw9523_ns = cg.esphome_ns.namespace(CONF_AW9523)
AW9523Component = aw9523_ns.class_("AW9523Component", cg.Component, i2c.I2CDevice)
AW9523GPIOPin = aw9523_ns.class_("AW9523GPIOPin", cg.GPIOPin)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(AW9523Component),
            cv.Optional(CONF_DIV, default=0): cv.int_range(min=0, max=3),
            cv.Optional(CONF_LATCH_INPUTS, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x58))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_divider(config[CONF_DIV]))
    cg.add(var.set_latch_inputs(config[CONF_LATCH_INPUTS]))
    return var


AW9523_PIN_SCHEMA = pins.gpio_base_schema(
    pin_type=AW9523GPIOPin, number_validator=cv.int_range(min=0, max=15)
).extend({cv.Required(CONF_AW9523): cv.use_id(AW9523Component)})


@pins.PIN_SCHEMA_REGISTRY.register(CONF_AW9523, AW9523_PIN_SCHEMA)
async def aw9523_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_AW9523])
    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
