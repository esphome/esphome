import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT
from esphomeyaml.helpers import add

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

}).extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA.schema)


def setup_light_component(obj, config):
    if CONF_DEFAULT_TRANSITION_LENGTH in config:
        add(obj.set_default_transition_length(config[CONF_DEFAULT_TRANSITION_LENGTH]))
    if CONF_GAMMA_CORRECT in config:
        add(obj.set_gamma_correct(config[CONF_GAMMA_CORRECT]))


BUILD_FLAGS = '-DUSE_LIGHT'
