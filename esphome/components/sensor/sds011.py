import voluptuous as vol

from esphome.components import sensor, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
from esphome.const import (CONF_ID, CONF_NAME, CONF_PM_10_0, CONF_PM_2_5, CONF_RX_ONLY,
                           CONF_UART_ID, CONF_UPDATE_INTERVAL)
from esphome.cpp_generator import Pvariable, add, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['uart']

SDS011Component = sensor.sensor_ns.class_('SDS011Component', uart.UARTDevice, Component)
SDS011Sensor = sensor.sensor_ns.class_('SDS011Sensor', sensor.EmptySensor)


def validate_sds011_rx_mode(value):
    if CONF_UPDATE_INTERVAL in value and not value.get(CONF_RX_ONLY):
        update_interval = value[CONF_UPDATE_INTERVAL]
        if update_interval.total_minutes > 30:
            raise vol.Invalid("Maximum update interval is 30min")
    elif value.get(CONF_RX_ONLY) and CONF_UPDATE_INTERVAL in value:
        # update_interval does not affect anything in rx-only mode, let's warn user about
        # that
        raise vol.Invalid("update_interval has no effect in rx_only mode. Please remove it.",
                          path=['update_interval'])
    return value


SDS011_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SDS011Sensor),
})

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SDS011Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),

    vol.Optional(CONF_RX_ONLY): cv.boolean,

    vol.Optional(CONF_PM_2_5): cv.nameable(SDS011_SENSOR_SCHEMA),
    vol.Optional(CONF_PM_10_0): cv.nameable(SDS011_SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_minutes,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(CONF_PM_2_5, CONF_PM_10_0),
                          validate_sds011_rx_mode)


def to_code(config):
    for uart_ in get_variable(config[CONF_UART_ID]):
        yield

    rhs = App.make_sds011(uart_)
    sds011 = Pvariable(config[CONF_ID], rhs)

    if CONF_UPDATE_INTERVAL in config:
        add(sds011.set_update_interval_min(config.get(CONF_UPDATE_INTERVAL)))
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
