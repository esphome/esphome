import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PM_10_0, CONF_PM_2_5, CONF_NAME, \
    CONF_UPDATE_INTERVAL, CONF_TIMEOUT
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, gpio_output_pin_expression, \
    setup_component
from esphome.cpp_types import App


Ppd42xSensorComponent = sensor.sensor_ns.class_('Ppd42xSensorComponent',
                                                sensor.PollingSensorComponent)


PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Ppd42xSensorComponent),
    vol.Optional(CONF_PM_10_0): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_PM_2_5):  pins.internal_gpio_input_pin_schema,
    vol.Required(CONF_TIMEOUT): cv.positive_time_period_microseconds,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    for pm_10_0 in gpio_input_pin_expression(config[CONF_PM_10_0]):
        yield
    for pm_02_5 in gpio_input_pin_expression(config[CONF_PM_2_5]):
        yield
    rhs = App.make_ppd42x_sensor(config[CONF_NAME], pm_10_0, pm_02_5,
                                 config.get(CONF_UPDATE_INTERVAL))
    ppd42x = Pvariable(config[CONF_ID], rhs)

    if CONF_TIMEOUT in config:
        add(ppd42x.set_timeout_us(config[CONF_TIMEOUT]))

    if CONF_PM_2_5 in config:
        conf = config[CONF_PM_2_5]
        sensor.register_sensor(ppd42x.make_pm_02_5_sensor(conf[CONF_NAME]), conf)
    if CONF_PM_10_0 in config:
        conf = config[CONF_PM_10_0]
        sensor.register_sensor(ppd42x.make_pm_10_0_sensor(conf[CONF_NAME]), conf)

    sensor.setup_sensor(ppd42x, config)
    setup_component(ppd42x, config)


BUILD_FLAGS = '-DUSE_PPD42X_SENSOR'
