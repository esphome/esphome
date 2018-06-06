import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_CARRIER_DUTY_PERCENT, CONF_ID, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, gpio_output_pin_expression

IRTransmitterComponent = switch.switch_ns.namespace('IRTransmitterComponent')

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(IRTransmitterComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_CARRIER_DUTY_PERCENT): vol.All(vol.Coerce(int),
                                                     vol.Range(min=1, max=100)),
})])


def to_code(config):
    for conf in config:
        pin = None
        for pin in gpio_output_pin_expression(conf[CONF_PIN]):
            yield
        rhs = App.make_ir_transmitter(pin, conf.get(CONF_CARRIER_DUTY_PERCENT))
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_IR_TRANSMITTER'
