from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

StatusBinarySensor = binary_sensor.binary_sensor_ns.class_('StatusBinarySensor',
                                                           binary_sensor.BinarySensor,
                                                           Component)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(StatusBinarySensor),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_status_binary_sensor(config[CONF_NAME])
    status = Pvariable(config[CONF_ID], rhs)
    binary_sensor.setup_binary_sensor(status, config)
    setup_component(status, config)


BUILD_FLAGS = '-DUSE_STATUS_BINARY_SENSOR'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
