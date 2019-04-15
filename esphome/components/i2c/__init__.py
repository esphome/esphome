import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_SCAN, CONF_SCL, CONF_SDA, CONF_ADDRESS, \
    CONF_I2C_ID
from esphome.core import coroutine

i2c_ns = cg.esphome_ns.namespace('i2c')
I2CComponent = i2c_ns.class_('I2CComponent', cg.Component)
I2CDevice = i2c_ns.class_('I2CDevice')

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(I2CComponent),
    cv.Optional(CONF_SDA, default='SDA'): pins.input_pin,
    cv.Optional(CONF_SCL, default='SCL'): pins.input_pin,
    cv.Optional(CONF_FREQUENCY, default='50kHz'):
        cv.All(cv.frequency, cv.Range(min=0, min_included=False)),
    cv.Optional(CONF_SCAN, default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_SDA], config[CONF_SCL],
                           config[CONF_FREQUENCY], config[CONF_SCAN])
    yield cg.register_component(var, config)

    cg.add_library('Wire', None)
    cg.add_global(i2c_ns.using)


def i2c_device_schema(default_address):
    schema = {
        cv.GenerateID(CONF_I2C_ID): cv.use_variable_id(I2CComponent),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.i2c_address
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.i2c_address
    return cv.Schema(schema)


@coroutine
def register_i2c_device(var, config):
    parent = yield cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_parent(parent))
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))
