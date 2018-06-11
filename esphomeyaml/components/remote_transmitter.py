import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_CARRIER_DUTY_PERCENT, CONF_ID, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, gpio_output_pin_expression

remote_ns = esphomelib_ns.namespace('remote')

RemoteTransmitterComponent = remote_ns.RemoteTransmitterComponent

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(RemoteTransmitterComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_CARRIER_DUTY_PERCENT): vol.All(cv.percentage_int,
                                                     vol.Range(min=1, max=100)),
})])


def to_code(config):
    for conf in config:
        pin = None
        for pin in gpio_output_pin_expression(conf[CONF_PIN]):
            yield
        rhs = App.make_remote_transmitter_component(pin)
        transmitter = Pvariable(conf[CONF_ID], rhs)
        if CONF_CARRIER_DUTY_PERCENT in conf:
            add(transmitter.set_carrier_duty_percent(conf[CONF_CARRIER_DUTY_PERCENT]))


BUILD_FLAGS = '-DUSE_REMOTE_TRANSMITTER'
