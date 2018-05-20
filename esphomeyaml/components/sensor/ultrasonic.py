import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ECHO_PIN, CONF_MAKE_ID, CONF_NAME, CONF_TIMEOUT_METER, \
    CONF_TIMEOUT_TIME, CONF_TRIGGER_PIN, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, gpio_input_pin_expression, \
    gpio_output_pin_expression, variable

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('ultrasonic', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_TRIGGER_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Required(CONF_ECHO_PIN): pins.GPIO_INTERNAL_INPUT_PIN_SCHEMA,
    vol.Exclusive(CONF_TIMEOUT_METER, 'timeout'): cv.positive_float,
    vol.Exclusive(CONF_TIMEOUT_TIME, 'timeout'): cv.positive_time_period_microseconds,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.SENSOR_SCHEMA.schema)

MakeUltrasonicSensor = Application.MakeUltrasonicSensor


def to_code(config):
    trigger = gpio_output_pin_expression(config[CONF_TRIGGER_PIN])
    echo = gpio_input_pin_expression(config[CONF_ECHO_PIN])
    rhs = App.make_ultrasonic_sensor(config[CONF_NAME], trigger, echo,
                                     config.get(CONF_UPDATE_INTERVAL))
    make = variable(MakeUltrasonicSensor, config[CONF_MAKE_ID], rhs)
    ultrasonic = make.Pultrasonic
    if CONF_TIMEOUT_TIME in config:
        add(ultrasonic.set_timeout_us(config[CONF_TIMEOUT_TIME]))
    elif CONF_TIMEOUT_METER in config:
        add(ultrasonic.set_timeout_m(config[CONF_TIMEOUT_METER]))
    sensor.setup_sensor(ultrasonic, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_ULTRASONIC_SENSOR'
