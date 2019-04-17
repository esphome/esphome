import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN, CONF_UPDATE_INTERVAL

MULTI_CONF = True
AUTO_LOAD = ['sensor']

CONF_ONE_WIRE_ID = 'one_wire_id'
dallas_ns = cg.esphome_ns.namespace('dallas')
DallasComponent = dallas_ns.class_('DallasComponent', cg.PollingComponent)
ESPOneWire = dallas_ns.class_('ESPOneWire')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(DallasComponent),
    cv.GenerateID(CONF_ONE_WIRE_ID): cv.declare_variable_id(ESPOneWire),
    cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    one_wire = cg.new_Pvariable(config[CONF_ONE_WIRE_ID], pin)
    var = cg.new_Pvariable(config[CONF_ID], one_wire, config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
