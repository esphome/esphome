import voluptuous as vol

from esphome.components import sensor, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
from esphome.const import (CONF_ID, CONF_NAME, CONF_PM_10_0, CONF_PM_2_5, CONF_UART_ID,
                           CONF_UPDATE_INTERVAL, CONF_QUERY_MODE, CONF_RX_ONLY)
from esphome.core import TimePeriod
from esphome.cpp_generator import Pvariable, get_variable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['uart']

SDS011Component = sensor.sensor_ns.class_('SDS011Component', uart.UARTDevice, Component)
SDS011Sensor = sensor.sensor_ns.class_('SDS011Sensor', sensor.EmptySensor)


def validate_sds011_rx_mode(value):
    if value.get(CONF_QUERY_MODE) and value.get(CONF_RX_ONLY):
        raise vol.Invalid(u"query_mode and rx_only can not be enabled on the same time!")
    if CONF_UPDATE_INTERVAL in value and not value.get(CONF_RX_ONLY):
        update_interval = value[CONF_UPDATE_INTERVAL]
        if isinstance(update_interval, TimePeriod):
            # Check for TimePeriod instance ('never' update interval)
            if (update_interval.milliseconds or 0) != 0 or (update_interval.seconds or 0) != 0:
                # Check if time period is multiple of minutes
                raise vol.Invalid("Maximum update interval precision in non-rx_only mode is 1min")
    return value


PMSX003_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SDS011Sensor),
})

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SDS011Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),

    vol.Optional(CONF_QUERY_MODE): cv.boolean,
    vol.Optional(CONF_RX_ONLY): cv.boolean,

    vol.Optional(CONF_PM_2_5): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_PM_10_0): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(CONF_PM_2_5, CONF_PM_10_0),
                          validate_sds011_rx_mode)


def to_code(config):
    for uart_ in get_variable(config[CONF_UART_ID]):
        yield

    rhs = App.make_sds011(uart_)
    sds011 = Pvariable(config[CONF_ID], rhs)

    if CONF_UPDATE_INTERVAL in config:
        add(sds011.set_update_interval(config.get(CONF_UPDATE_INTERVAL)))
    if CONF_QUERY_MODE in config:
        add(sds011.set_query_mode(config[CONF_QUERY_MODE]))
    if CONF_RX_ONLY in config:
        add(sds011.set_rx_mode_only(config[CONF_RX_ONLY]))

    if CONF_PM_2_5 in config:
        conf = config[CONF_PM_2_5]
        sensor.register_sensor(sds011.make_pm_2_5_sensor(conf[CONF_NAME]), conf)
    if CONF_PM_10_0 in config:
        conf = config[CONF_PM_10_0]
        sensor.register_sensor(sds011.make_pm_10_0_sensor(conf[CONF_NAME]), conf)

    setup_component(sds011, config)


BUILD_FLAGS = '-DUSE_SDS011'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_PM_2_5, CONF_PM_10_0):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
