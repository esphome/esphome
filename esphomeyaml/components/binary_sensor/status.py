import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_ID, CONF_NAME
from esphomeyaml.helpers import App, variable

DEPENDENCIES = ['mqtt']

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('status_binary_sensor'): cv.register_variable_id,
}).extend(binary_sensor.MQTT_BINARY_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_status_binary_sensor(config[CONF_NAME])
    status = variable('Application::MakeStatusBinarySensor', config[CONF_ID], rhs)
    binary_sensor.setup_binary_sensor(status.Pstatus, config)
    binary_sensor.setup_mqtt_binary_sensor(status.Pmqtt, config)


BUILD_FLAGS = '-DUSE_STATUS_BINARY_SENSOR'
