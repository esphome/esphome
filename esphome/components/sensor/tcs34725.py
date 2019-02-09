# coding=utf-8
import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_COLOR_TEMPERATURE, CONF_GAIN, CONF_ID, \
    CONF_ILLUMINANCE, CONF_INTEGRATION_TIME, CONF_NAME, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

CONF_RED_CHANNEL = 'red_channel'
CONF_GREEN_CHANNEL = 'green_channel'
CONF_BLUE_CHANNEL = 'blue_channel'
CONF_CLEAR_CHANNEL = 'clear_channel'

TCS34725Component = sensor.sensor_ns.class_('TCS34725Component', PollingComponent,
                                            i2c.I2CDevice)

TCS34725IntegrationTime = sensor.sensor_ns.enum('TCS34725IntegrationTime')
TCS34725_INTEGRATION_TIMES = {
    '2.4ms': TCS34725IntegrationTime.TCS34725_INTEGRATION_TIME_2_4MS,
    '24ms': TCS34725IntegrationTime.TCS34725_INTEGRATION_TIME_24MS,
    '50ms': TCS34725IntegrationTime.TCS34725_INTEGRATION_TIME_50MS,
    '101ms': TCS34725IntegrationTime.TCS34725_INTEGRATION_TIME_101MS,
    '154ms': TCS34725IntegrationTime.TCS34725_INTEGRATION_TIME_154MS,
    '700ms': TCS34725IntegrationTime.TCS34725_INTEGRATION_TIME_700MS,
}

TCS34725Gain = sensor.sensor_ns.enum('TCS34725Gain')
TCS34725_GAINS = {
    '1X': TCS34725Gain.TCS34725_GAIN_1X,
    '4X': TCS34725Gain.TCS34725_GAIN_4X,
    '16X': TCS34725Gain.TCS34725_GAIN_16X,
    '60X': TCS34725Gain.TCS34725_GAIN_60X,
}

TCS35725IlluminanceSensor = sensor.sensor_ns.class_('TCS35725IlluminanceSensor',
                                                    sensor.EmptyPollingParentSensor)
TCS35725ColorTemperatureSensor = sensor.sensor_ns.class_('TCS35725ColorTemperatureSensor',
                                                         sensor.EmptyPollingParentSensor)
TCS35725ColorChannelSensor = sensor.sensor_ns.class_('TCS35725ColorChannelSensor',
                                                     sensor.EmptyPollingParentSensor)

COLOR_CHANNEL_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TCS35725ColorChannelSensor),
})

SENSOR_KEYS = [CONF_RED_CHANNEL, CONF_GREEN_CHANNEL, CONF_BLUE_CHANNEL,
               CONF_CLEAR_CHANNEL, CONF_ILLUMINANCE, CONF_COLOR_TEMPERATURE]

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TCS34725Component),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_RED_CHANNEL): cv.nameable(COLOR_CHANNEL_SENSOR_SCHEMA),
    vol.Optional(CONF_GREEN_CHANNEL): cv.nameable(COLOR_CHANNEL_SENSOR_SCHEMA),
    vol.Optional(CONF_BLUE_CHANNEL): cv.nameable(COLOR_CHANNEL_SENSOR_SCHEMA),
    vol.Optional(CONF_CLEAR_CHANNEL): cv.nameable(COLOR_CHANNEL_SENSOR_SCHEMA),
    vol.Optional(CONF_ILLUMINANCE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(TCS35725IlluminanceSensor),
    })),
    vol.Optional(CONF_COLOR_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(TCS35725ColorTemperatureSensor),
    })),
    vol.Optional(CONF_INTEGRATION_TIME): cv.one_of(*TCS34725_INTEGRATION_TIMES, lower=True),
    vol.Optional(CONF_GAIN): vol.All(vol.Upper, cv.one_of(*TCS34725_GAINS), upper=True),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSOR_KEYS))


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

    setup_component(tcs, config)


BUILD_FLAGS = '-DUSE_TCS34725'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_RED_CHANNEL, CONF_GREEN_CHANNEL, CONF_BLUE_CHANNEL, CONF_CLEAR_CHANNEL,
                CONF_ILLUMINANCE, CONF_COLOR_TEMPERATURE):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
