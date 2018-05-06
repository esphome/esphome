import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ECHO_PIN, CONF_ID, CONF_NAME, \
    CONF_TIMEOUT_METER, CONF_TIMEOUT_TIME, CONF_TRIGGER_PIN, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, add, exp_gpio_input_pin, exp_gpio_output_pin, \
    variable

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('ultrasonic'): cv.register_variable_id,
    vol.Required(CONF_TRIGGER_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Required(CONF_ECHO_PIN): pins.GPIO_INPUT_PIN_SCHEMA,
    vol.Exclusive(CONF_TIMEOUT_METER, 'timeout'): cv.positive_float,
    vol.Exclusive(CONF_TIMEOUT_TIME, 'timeout'): cv.positive_int,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
}).extend(sensor.MQTT_SENSOR_SCHEMA.schema)


def to_code(config):
    trigger = exp_gpio_output_pin(config[CONF_TRIGGER_PIN])
    echo = exp_gpio_input_pin(config[CONF_ECHO_PIN])
    rhs = App.make_ultrasonic_sensor(config[CONF_NAME], trigger, echo,
                                     config.get(CONF_UPDATE_INTERVAL))
    make = variable('Application::MakeUltrasonicSensor', config[CONF_ID], rhs)
    ultrasonic = make.Pultrasonic
    if CONF_TIMEOUT_TIME in config:
        add(ultrasonic.set_timeout_us(config[CONF_TIMEOUT_TIME]))
    elif CONF_TIMEOUT_METER in config:
        add(ultrasonic.set_timeout_m(config[CONF_TIMEOUT_METER]))
    sensor.setup_sensor(ultrasonic, config)
    sensor.setup_mqtt_sensor_component(make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_ULTRASONIC_SENSOR'
