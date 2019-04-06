import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PM_10_0, \
    CONF_PM_2_5, CONF_PIN, CONF_TYPE, CONF_UPDATE_INTERVAL, CONF_TIMEOUT
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component


PPD42XComponent = sensor.sensor_ns.class_('PPD42XComponent', Component)
PPD42XSensor = sensor.sensor_ns.class_('PPD42XSensor', sensor.Sensor)

CONF_PPD42__ = 'PPD42'
CONF_PPD42NS = 'PPD42NS'

PPD42XType = sensor.sensor_ns.enum('PPD42XType')
PPD42X_TYPES = {
    CONF_PPD42__: PPD42XType.PPD42X_TYPE___,
    CONF_PPD42NS: PPD42XType.PPD42X_TYPE_NS,
}

SENSORS_TO_TYPE = {
    CONF_PM_2_5:  [CONF_PPD42__, CONF_PPD42NS],
    CONF_PM_10_0: [CONF_PPD42__, CONF_PPD42NS],
}


def validate_PPD42X_sensors(value):
    for key, types in SENSORS_TO_TYPE.items():
        if key in value and value[CONF_TYPE] not in types:
            raise vol.Invalid(u"{} does not have {} sensor!".format(value[CONF_TYPE], key))
    return value


PPD42X_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PPD42XSensor),
    vol.Required(CONF_PIN): pins.gpio_input_pin_schema
})

PLATFORM_SCHEMA = cv.nameable(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PPD42XComponent),
    vol.Required(CONF_TYPE): cv.one_of(*PPD42X_TYPES, upper=True),
    vol.Optional(CONF_PM_2_5): cv.nameable(PPD42X_SENSOR_SCHEMA),
    vol.Optional(CONF_PM_10_0): cv.nameable(PPD42X_SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_TIMEOUT): cv.positive_time_period_microseconds,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSORS_TO_TYPE))


def to_code(config):
    rhs = App.make_ppd42x(PPD42X_TYPES[config[CONF_TYPE]])
    ppd = Pvariable(config[CONF_ID], rhs)
    if CONF_PM_2_5 in config:
        conf = config[CONF_PM_2_5]
        for pm_02_5 in get_variable(conf[CONF_PIN]):
            yield
        sensor.register_sensor(ppd.make_pm_02_5_sensor(conf[CONF_NAME]), pm_02_5)
    if CONF_PM_10_0 in config:
        conf = config[CONF_PM_10_0]
        for pm_10_0 in get_variable(conf[CONF_PIN]):
            yield
        sensor.register_sensor(ppd.make_pm_10_0_sensor(conf[CONF_NAME]), pm_10_0)
    setup_component(ppd, config)


BUILD_FLAGS = '-DUSE_PPD42X'
