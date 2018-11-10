import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable, setup_component, Component

DEPENDENCIES = ['mqtt']

MakeStatusBinarySensor = Application.struct('MakeStatusBinarySensor')
StatusBinarySensor = binary_sensor.binary_sensor_ns.class_('StatusBinarySensor',
                                                           binary_sensor.BinarySensor,
                                                           Component)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeStatusBinarySensor),
    cv.GenerateID(): cv.declare_variable_id(StatusBinarySensor),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_status_binary_sensor(config[CONF_NAME])
    status = variable(config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(status.Pstatus, status.Pmqtt, config)
    setup_component(status.Pstatus, config)


BUILD_FLAGS = '-DUSE_STATUS_BINARY_SENSOR'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
