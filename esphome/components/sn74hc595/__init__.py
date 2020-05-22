import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID, CONF_NUMBER, CONF_INVERTED

DEPENDENCIES = ['spi']
MULTI_CONF = True

sn74hc595_ns = cg.esphome_ns.namespace('sn74hc595')

sn74hc595 = sn74hc595_ns.class_('SN74HC595', cg.Component, spi.SPIDevice)
sn74hc595GPIOPin = sn74hc595_ns.class_('SN74HC595GpioPin', cg.GPIOPin)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(sn74hc595),
}).extend(cv.COMPONENT_SCHEMA).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)


sn74hc595_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.Required('sn74hc595'): cv.use_id(sn74hc595),
    cv.Required(CONF_NUMBER): cv.int_range(0, 7),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})

sn74hc595_INPUT_PIN_SCHEMA = cv.Schema({})


@pins.PIN_SCHEMA_REGISTRY.register('sn74hc595', (sn74hc595_OUTPUT_PIN_SCHEMA,
                                                 sn74hc595_INPUT_PIN_SCHEMA))
def sn74hc595_pin_to_code(config):
    parent = yield cg.get_variable(config['sn74hc595'])
    yield sn74hc595GPIOPin.new(parent, config[CONF_NUMBER], config[CONF_INVERTED])
