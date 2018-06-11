import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

DEPENDENCIES = ['mqtt']

MakeStatusBinarySensor = Application.MakeStatusBinarySensor

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeStatusBinarySensor),
}))


def to_code(config):
    rhs = App.make_status_binary_sensor(config[CONF_NAME])
    status = variable(config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(status.Pstatus, status.Pmqtt, config)


BUILD_FLAGS = '-DUSE_STATUS_BINARY_SENSOR'
