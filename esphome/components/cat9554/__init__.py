import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_NUMBER, CONF_MODE, CONF_INVERTED

DEPENDENCIES = ['i2c']
MULTI_CONF = True

cat9554_ns = cg.esphome_ns.namespace('cat9554')
CAT9554GPIOMode = cat9554_ns.enum('CAT9554GPIOMode')
CAT9554_GPIO_MODES = {
    'INPUT': CAT9554GPIOMode.CAT9554_INPUT,
    'OUTPUT': CAT9554GPIOMode.CAT9554_OUTPUT,
}

CAT9554Component = cat9554_ns.class_('CAT9554Component', cg.Component, i2c.I2CDevice)
CAT9554GPIOPin = cat9554_ns.class_('CAT9554GPIOPin', cg.GPIOPin)

CONF_CAT9554 = 'cat9554'
CONF_IRQ_PIN = 'irq_pin'
CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(CAT9554Component),
    cv.Required(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x20))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    irq_pin = yield cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(irq_pin))
    yield i2c.register_i2c_device(var, config)


def validate_cat9554_gpio_mode(value):
    value = cv.string(value)
    if value.upper() == 'INPUT_PULLUP':
        raise cv.Invalid("INPUT_PULLUP mode has been removed in 1.14 and been combined into "
                         "INPUT mode (they were the same thing). Please use INPUT instead.")
    return cv.enum(CAT9554_GPIO_MODES, upper=True)(value)


CAT9554_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_CAT9554): cv.use_id(CAT9554Component),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="OUTPUT"): validate_cat9554_gpio_mode,
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})
CAT9554_INPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_CAT9554): cv.use_id(CAT9554Component),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="INPUT"): validate_cat9554_gpio_mode,
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})


@pins.PIN_SCHEMA_REGISTRY.register('cat9554', (CAT9554_OUTPUT_PIN_SCHEMA, CAT9554_INPUT_PIN_SCHEMA))
def cat9554_pin_to_code(config):
    parent = yield cg.get_variable(config[CONF_CAT9554])
    yield CAT9554GPIOPin.new(parent, config[CONF_NUMBER], config[CONF_MODE], config[CONF_INVERTED])
