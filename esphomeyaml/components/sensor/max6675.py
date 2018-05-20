import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN_CLOCK, CONF_PIN_CS, CONF_PIN_MISO, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, gpio_input_pin_expression, \
    gpio_output_pin_expression, variable

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('max6675', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_PIN_CS): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Required(CONF_PIN_CLOCK): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Optional(CONF_PIN_MISO): pins.GPIO_INPUT_PIN_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.SENSOR_SCHEMA.schema)

MakeMAX6675Sensor = Application.MakeMAX6675Sensor


def to_code(config):
    pin_cs = gpio_output_pin_expression(config[CONF_PIN_CS])
    pin_clock = gpio_output_pin_expression(config[CONF_PIN_CLOCK])
    pin_miso = gpio_input_pin_expression(config[CONF_PIN_MISO])
    rhs = App.make_max6675_sensor(config[CONF_NAME], pin_cs, pin_clock, pin_miso,
                                  config.get(CONF_UPDATE_INTERVAL))
    make = variable(MakeMAX6675Sensor, config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Pmax6675, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_MAX6675_SENSOR'
