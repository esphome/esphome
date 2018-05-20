import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT, CONF_ID, \
    CONF_MQTT_ID
from esphomeyaml.helpers import Application, Pvariable, add, esphomelib_ns, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

LIGHT_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID('light'): cv.register_variable_id,
    cv.GenerateID('mqtt_light', CONF_MQTT_ID): cv.register_variable_id,
})

light_ns = esphomelib_ns.namespace('light')
LightState = light_ns.LightState
MQTTJSONLightComponent = light_ns.MQTTJSONLightComponent
ToggleAction = light_ns.ToggleAction
TurnOffAction = light_ns.TurnOffAction
TurnOnAction = light_ns.TurnOnAction
MakeLight = Application.MakeLight


def setup_light_core_(light_var, mqtt_var, config):
    if CONF_DEFAULT_TRANSITION_LENGTH in config:
        add(light_var.set_default_transition_length(config[CONF_DEFAULT_TRANSITION_LENGTH]))
    if CONF_GAMMA_CORRECT in config:
        add(light_var.set_gamma_correct(config[CONF_GAMMA_CORRECT]))

    setup_mqtt_component(mqtt_var, config)


def setup_light(light_obj, mqtt_obj, config):
    light_var = Pvariable(LightState, config[CONF_ID], light_obj, has_side_effects=False)
    mqtt_var = Pvariable(MQTTJSONLightComponent, config[CONF_MQTT_ID], mqtt_obj,
                         has_side_effects=False)
    setup_light_core_(light_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_LIGHT'
