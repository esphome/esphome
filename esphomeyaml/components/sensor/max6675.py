import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN_CLOCK, CONF_PIN_CS, CONF_PIN_MISO, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, gpio_input_pin_expression, \
    gpio_output_pin_expression, variable

MakeMAX6675Sensor = Application.MakeMAX6675Sensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMAX6675Sensor),
    vol.Required(CONF_PIN_CS): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Required(CONF_PIN_CLOCK): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Optional(CONF_PIN_MISO): pins.GPIO_INPUT_PIN_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.SENSOR_SCHEMA.schema)


def to_code(config):
    pin_cs = None
    for pin_cs in gpio_output_pin_expression(config[CONF_PIN_CS]):
        yield
    pin_clock = None
    for pin_clock in gpio_output_pin_expression(config[CONF_PIN_CLOCK]):
        yield
    pin_miso = None
    for pin_miso in gpio_input_pin_expression(config[CONF_PIN_MISO]):
        yield
    rhs = App.make_max6675_sensor(config[CONF_NAME], pin_cs, pin_clock, pin_miso,
                                  config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Pmax6675, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_MAX6675_SENSOR'
