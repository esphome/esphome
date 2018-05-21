import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_MQTT_ID
from esphomeyaml.helpers import Pvariable, esphomelib_ns, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

COVER_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID('cover'): cv.register_variable_id,
    cv.GenerateID('mqtt_cover', CONF_MQTT_ID): cv.register_variable_id,
})

cover_ns = esphomelib_ns.namespace('cover')
Cover = cover_ns.Cover
MQTTCoverComponent = cover_ns.MQTTCoverComponent
CoverState = cover_ns.CoverState
COVER_OPEN = cover_ns.COVER_OPEN
COVER_CLOSED = cover_ns.COVER_CLOSED
OpenAction = cover_ns.OpenAction
CloseAction = cover_ns.CloseAction
StopAction = cover_ns.StopAction


def setup_cover_core_(cover_var, mqtt_var, config):
    setup_mqtt_component(mqtt_var, config)


def setup_cover(cover_obj, mqtt_obj, config):
    cover_var = Pvariable(Cover, config[CONF_ID], cover_obj, has_side_effects=False)
    mqtt_var = Pvariable(MQTTCoverComponent, config[CONF_MQTT_ID], mqtt_obj,
                         has_side_effects=False)
    setup_cover_core_(cover_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_COVER'
