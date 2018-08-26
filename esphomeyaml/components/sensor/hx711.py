import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_GAIN, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL, CONF_CLK_PIN
from esphomeyaml.helpers import App, Application, add, gpio_input_pin_expression, variable

MakeHX711Sensor = Application.MakeHX711Sensor

CONF_DOUT_PIN = 'dout_pin'

GAINS = {
    128: sensor.sensor_ns.HX711_GAIN_128,
    32: sensor.sensor_ns.HX711_GAIN_32,
    64: sensor.sensor_ns.HX711_GAIN_64,
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeHX711Sensor),
    vol.Required(CONF_DOUT_PIN): pins.gpio_input_pin_schema,
    vol.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_GAIN): vol.All(cv.int_, cv.one_of(*GAINS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    dout_pin = None
    for dout_pin in gpio_input_pin_expression(config[CONF_DOUT_PIN]):
        yield
    sck_pin = None
    for sck_pin in gpio_input_pin_expression(config[CONF_CLK_PIN]):
        yield

    rhs = App.make_hx711_sensor(config[CONF_NAME], dout_pin, sck_pin,
                                config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)

    if CONF_GAIN in config:
        add(make.Phx711.set_gain(GAINS[config[CONF_GAIN]]))

    sensor.setup_sensor(make.Phx711, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_HX711'
