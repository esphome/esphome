# coding=utf-8
import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_COLOR_TEMPERATURE, CONF_GAIN, CONF_ID, \
    CONF_ILLUMINANCE, CONF_INTEGRATION_TIME, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Pvariable, add

DEPENDENCIES = ['i2c']

CONF_RED_CHANNEL = 'red_channel'
CONF_GREEN_CHANNEL = 'green_channel'
CONF_BLUE_CHANNEL = 'blue_channel'
CONF_CLEAR_CHANNEL = 'clear_channel'

TCS34725Component = sensor.sensor_ns.TCS34725Component

TCS34725_INTEGRATION_TIMES = {
    '2.4ms': sensor.sensor_ns.TCS34725_INTEGRATION_TIME_2_4MS,
    '24ms': sensor.sensor_ns.TCS34725_INTEGRATION_TIME_24MS,
    '50ms': sensor.sensor_ns.TCS34725_INTEGRATION_TIME_50MS,
    '101ms': sensor.sensor_ns.TCS34725_INTEGRATION_TIME_101MS,
    '154ms': sensor.sensor_ns.TCS34725_INTEGRATION_TIME_154MS,
    '700ms': sensor.sensor_ns.TCS34725_INTEGRATION_TIME_700MS,
}

TCS34725_GAINS = {
    '1X': sensor.sensor_ns.TCS34725_GAIN_1X,
    '4X': sensor.sensor_ns.TCS34725_GAIN_4X,
    '16X': sensor.sensor_ns.TCS34725_GAIN_16X,
    '60X': sensor.sensor_ns.TCS34725_GAIN_60X,
}

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TCS34725Component),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_RED_CHANNEL): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_GREEN_CHANNEL): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_BLUE_CHANNEL): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_CLEAR_CHANNEL): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_ILLUMINANCE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_COLOR_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_INTEGRATION_TIME): cv.one_of(*TCS34725_INTEGRATION_TIMES),
    vol.Optional(CONF_GAIN): vol.All(vol.Upper, cv.one_of(*TCS34725_GAINS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}), cv.has_at_least_one_key(CONF_RED_CHANNEL, CONF_GREEN_CHANNEL, CONF_BLUE_CHANNEL,
                            CONF_CLEAR_CHANNEL, CONF_ILLUMINANCE, CONF_COLOR_TEMPERATURE))


def to_code(config):
    rhs = App.make_tcs34725(config.get(CONF_UPDATE_INTERVAL))
    tcs = Pvariable(config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        add(tcs.set_address(config[CONF_ADDRESS]))
    if CONF_INTEGRATION_TIME in config:
        add(tcs.set_integration_time(TCS34725_INTEGRATION_TIMES[config[CONF_INTEGRATION_TIME]]))
    if CONF_GAIN in config:
        add(tcs.set_gain(TCS34725_GAINS[config[CONF_GAIN]]))
    if CONF_RED_CHANNEL in config:
        conf = config[CONF_RED_CHANNEL]
        sensor.register_sensor(tcs.Pmake_red_sensor(conf[CONF_NAME]), conf)
    if CONF_GREEN_CHANNEL in config:
        conf = config[CONF_GREEN_CHANNEL]
        sensor.register_sensor(tcs.Pmake_green_sensor(conf[CONF_NAME]), conf)
    if CONF_BLUE_CHANNEL in config:
        conf = config[CONF_BLUE_CHANNEL]
        sensor.register_sensor(tcs.Pmake_blue_sensor(conf[CONF_NAME]), conf)
    if CONF_CLEAR_CHANNEL in config:
        conf = config[CONF_CLEAR_CHANNEL]
        sensor.register_sensor(tcs.Pmake_clear_sensor(conf[CONF_NAME]), conf)
    if CONF_ILLUMINANCE in config:
        conf = config[CONF_ILLUMINANCE]
        sensor.register_sensor(tcs.Pmake_illuminance_sensor(conf[CONF_NAME]), conf)
    if CONF_COLOR_TEMPERATURE in config:
        conf = config[CONF_COLOR_TEMPERATURE]
        sensor.register_sensor(tcs.Pmake_color_temperature_sensor(conf[CONF_NAME]), conf)


BUILD_FLAGS = '-DUSE_TCS34725'
