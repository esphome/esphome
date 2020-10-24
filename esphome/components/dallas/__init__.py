import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN, CONF_PIN_A

MULTI_CONF = True
AUTO_LOAD = ['sensor']

CONF_ONE_WIRE_ID = 'one_wire_id'
CONF_SHELLY_ONE_WIRE_ID = 'shelly_one_wire_id'
dallas_ns = cg.esphome_ns.namespace('dallas')
DallasComponent = dallas_ns.class_('DallasComponent', cg.PollingComponent)
ESPOneWire = dallas_ns.class_('ESPOneWire')
ShellyOneWire = dallas_ns.class_('ShellyOneWire')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DallasComponent),
    cv.GenerateID(CONF_ONE_WIRE_ID): cv.declare_id(ESPOneWire),
    cv.GenerateID(CONF_SHELLY_ONE_WIRE_ID): cv.declare_id(ShellyOneWire),
    cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_PIN_A): pins.gpio_output_pin_schema,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    if CONF_PIN_A in config:
        pin_out = yield cg.gpio_pin_expression(config[CONF_PIN_A])
        one_wire = cg.new_Pvariable(config[CONF_SHELLY_ONE_WIRE_ID], pin, pin_out)
    else:
        one_wire = cg.new_Pvariable(config[CONF_ONE_WIRE_ID], pin)
    var = cg.new_Pvariable(config[CONF_ID], one_wire)
    yield cg.register_component(var, config)
