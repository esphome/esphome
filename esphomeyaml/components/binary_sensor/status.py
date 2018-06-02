import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

DEPENDENCIES = ['mqtt']

MakeStatusBinarySensor = Application.MakeStatusBinarySensor

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeStatusBinarySensor),
}).extend(binary_sensor.BINARY_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_status_binary_sensor(config[CONF_NAME])
    status = variable(config[CONF_MAKE_ID], rhs)
    for _ in binary_sensor.setup_binary_sensor(status.Pstatus, status.Pmqtt, config):
        yield


BUILD_FLAGS = '-DUSE_STATUS_BINARY_SENSOR'
