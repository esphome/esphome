import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

DEPENDENCIES = ['mqtt']

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('status_binary_sensor', CONF_MAKE_ID): cv.register_variable_id,
}).extend(binary_sensor.BINARY_SENSOR_SCHEMA.schema)

MakeStatusBinarySensor = Application.MakeStatusBinarySensor


def to_code(config):
    rhs = App.make_status_binary_sensor(config[CONF_NAME])
    status = variable(MakeStatusBinarySensor, config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(status.Pstatus, status.Pmqtt, config)


BUILD_FLAGS = '-DUSE_STATUS_BINARY_SENSOR'
