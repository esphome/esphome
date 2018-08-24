import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core, pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_COUNT_MODE, CONF_FALLING_EDGE, CONF_INTERNAL_FILTER, \
    CONF_MAKE_ID, CONF_NAME, CONF_PIN, CONF_PULL_MODE, CONF_RISING_EDGE, CONF_UPDATE_INTERVAL, \
    ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Application, add, variable, gpio_input_pin_expression

COUNT_MODES = {
    'DISABLE': sensor.sensor_ns.PULSE_COUNTER_DISABLE,
    'INCREMENT': sensor.sensor_ns.PULSE_COUNTER_INCREMENT,
    'DECREMENT': sensor.sensor_ns.PULSE_COUNTER_DECREMENT,
}

COUNT_MODE_SCHEMA = vol.All(vol.Upper, cv.one_of(*COUNT_MODES))

MakePulseCounterSensor = Application.MakePulseCounterSensor


def validate_internal_filter(value):
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        if isinstance(value, int):
            raise vol.Invalid("Please specify the internal filter in microseconds now "
                              "(since 1.7.0). For example '17ms'")
        value = cv.positive_time_period_microseconds(value)
        if value.total_microseconds > 13:
            raise vol.Invalid("Maximum internal filter value for ESP32 is 13us")
        return value
    else:
        return cv.positive_time_period_microseconds(value)


PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakePulseCounterSensor),
    vol.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_COUNT_MODE): vol.Schema({
        vol.Required(CONF_RISING_EDGE): COUNT_MODE_SCHEMA,
        vol.Required(CONF_FALLING_EDGE): COUNT_MODE_SCHEMA,
    }),
    vol.Optional(CONF_INTERNAL_FILTER): validate_internal_filter,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,

    vol.Optional(CONF_PULL_MODE): cv.invalid("The pull_mode option has been removed in 1.7.0, "
                                             "please use the pin mode schema now.")
}))


def to_code(config):
    pin = None
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_pulse_counter_sensor(config[CONF_NAME], pin,
                                        config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    pcnt = make.Ppcnt
    if CONF_COUNT_MODE in config:
        rising_edge = COUNT_MODES[config[CONF_COUNT_MODE][CONF_RISING_EDGE]]
        falling_edge = COUNT_MODES[config[CONF_COUNT_MODE][CONF_FALLING_EDGE]]
        add(pcnt.set_edge_mode(rising_edge, falling_edge))
    if CONF_INTERNAL_FILTER in config:
        add(pcnt.set_filter_us(config[CONF_INTERNAL_FILTER]))
    sensor.setup_sensor(make.Ppcnt, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_PULSE_COUNTER_SENSOR'
