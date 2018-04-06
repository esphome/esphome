import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_DEFAULT_TRANSITION_LENGTH
from esphomeyaml.helpers import add, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

}).extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA.schema)


def setup_mqtt_light_component(obj, config):
    if CONF_DEFAULT_TRANSITION_LENGTH in config:
        add(obj.set_default_transition_length(config[CONF_DEFAULT_TRANSITION_LENGTH]))
    setup_mqtt_component(obj, config)
