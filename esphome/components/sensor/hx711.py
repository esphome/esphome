import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CLK_PIN, CONF_GAIN, CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, setup_component
from esphome.cpp_types import App, Application

MakeHX711Sensor = Application.struct('MakeHX711Sensor')
HX711Sensor = sensor.sensor_ns.class_('HX711Sensor', sensor.PollingSensorComponent)

CONF_DOUT_PIN = 'dout_pin'

HX711Gain = sensor.sensor_ns.enum('HX711Gain')
GAINS = {
    128: HX711Gain.HX711_GAIN_128,
    32: HX711Gain.HX711_GAIN_32,
    64: HX711Gain.HX711_GAIN_64,
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HX711Sensor),
    vol.Required(CONF_DOUT_PIN): pins.gpio_input_pin_schema,
    vol.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_GAIN): cv.one_of(*GAINS, int=True),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for dout_pin in gpio_input_pin_expression(config[CONF_DOUT_PIN]):
        yield
    for sck_pin in gpio_input_pin_expression(config[CONF_CLK_PIN]):
        yield

    rhs = App.make_hx711_sensor(config[CONF_NAME], dout_pin, sck_pin,
                                config.get(CONF_UPDATE_INTERVAL))
    hx711 = Pvariable(config[CONF_ID], rhs)

    if CONF_GAIN in config:
        add(hx711.set_gain(GAINS[config[CONF_GAIN]]))

    sensor.setup_sensor(hx711, config)
    setup_component(hx711, config)


BUILD_FLAGS = '-DUSE_HX711'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
