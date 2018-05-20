import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_CARRIER_DUTY_PERCENT, CONF_ID, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, gpio_output_pin_expression

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID('ir_transmitter'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Optional(CONF_CARRIER_DUTY_PERCENT): vol.All(vol.Coerce(int),
                                                     vol.Range(min=1, max=100)),
})])

IRTransmitterComponent = switch.switch_ns.namespace('IRTransmitterComponent')


def to_code(config):
    for conf in config:
        pin = gpio_output_pin_expression(conf[CONF_PIN])
        rhs = App.make_ir_transmitter(pin, conf.get(CONF_CARRIER_DUTY_PERCENT))
        Pvariable(IRTransmitterComponent, conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_IR_TRANSMITTER'
