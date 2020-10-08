import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_SCAN, CONF_SCL, CONF_SDA, CONF_ADDRESS, \
    CONF_I2C_ID
from esphome.core import coroutine, coroutine_with_priority

CODEOWNERS = ['@esphome/core']
i2c_ns = cg.esphome_ns.namespace('i2c')
I2CComponent = i2c_ns.class_('I2CComponent', cg.Component)
I2CDevice = i2c_ns.class_('I2CDevice')

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(I2CComponent),
    cv.Optional(CONF_SDA, default='SDA'): pins.input_pin,
    cv.Optional(CONF_SCL, default='SCL'): pins.input_pin,
    cv.Optional(CONF_FREQUENCY, default='50kHz'):
        cv.All(cv.frequency, cv.Range(min=0, min_included=False)),
    cv.Optional(CONF_SCAN, default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(1.0)
def to_code(config):
    cg.add_global(i2c_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_sda_pin(config[CONF_SDA]))
    cg.add(var.set_scl_pin(config[CONF_SCL]))
    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_scan(config[CONF_SCAN]))
    cg.add_library('Wire', None)


def i2c_device_schema(default_address):
    """Create a schema for a i2c device.

    :param default_address: The default address of the i2c device, can be None to represent
      a required option.
    :return: The i2c device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CComponent),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.i2c_address
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.i2c_address
    return cv.Schema(schema)


@coroutine
def register_i2c_device(var, config):
    """Register an i2c device with the given config.

    Sets the i2c bus to use and the i2c address.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = yield cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_parent(parent))
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))
