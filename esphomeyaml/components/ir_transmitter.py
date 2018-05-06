import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_CARRIER_DUTY_PERCENT, CONF_ID, CONF_PIN, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Pvariable, exp_gpio_output_pin

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

IR_TRANSMITTER_COMPONENT_CLASS = 'switch_::IRTransmitterComponent'

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID('ir_transmitter'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Optional(CONF_CARRIER_DUTY_PERCENT): vol.All(vol.Coerce(int), vol.Range(min=0, max=100)),
})])


def to_code(config):
    for conf in config:
        pin = exp_gpio_output_pin(conf[CONF_PIN])
        rhs = App.make_ir_transmitter(pin, conf.get(CONF_CARRIER_DUTY_PERCENT))
        Pvariable(IR_TRANSMITTER_COMPONENT_CLASS, conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_IR_TRANSMITTER'
