from esphomeyaml.components import text_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

MakeVersionTextSensor = Application.MakeVersionTextSensor

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeVersionTextSensor),
}))


def to_code(config):
    rhs = App.make_version_text_sensor(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)
    text_sensor.setup_text_sensor(make.Psensor, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_VERSION_TEXT_SENSOR'
