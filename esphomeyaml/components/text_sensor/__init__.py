import voluptuous as vol

from esphomeyaml import automation
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ICON, CONF_ID, CONF_INTERNAL, CONF_MQTT_ID, CONF_ON_VALUE, \
    CONF_TRIGGER_ID
from esphomeyaml.helpers import App, Pvariable, add, add_job, esphomelib_ns, setup_mqtt_component, \
    std_string

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

# pylint: disable=invalid-name
text_sensor_ns = esphomelib_ns.namespace('text_sensor')
TextSensor = text_sensor_ns.TextSensor
MQTTTextSensor = text_sensor_ns.MQTTTextSensor

TextSensorStateTrigger = text_sensor_ns.TextSensorStateTrigger

TEXT_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTTextSensor),
    cv.GenerateID(): cv.declare_variable_id(TextSensor),
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_ON_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(TextSensorStateTrigger),
    }),
})

TEXT_SENSOR_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(TEXT_SENSOR_SCHEMA.schema)


def setup_text_sensor_core_(text_sensor_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(text_sensor_var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        add(text_sensor_var.set_icon(config[CONF_ICON]))

    for conf in config.get(CONF_ON_VALUE, []):
        rhs = text_sensor_var.make_state_trigger()
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, std_string, conf)

    setup_mqtt_component(mqtt_var, config)


def setup_text_sensor(text_sensor_obj, mqtt_obj, config):
    sensor_var = Pvariable(config[CONF_ID], text_sensor_obj, has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    add_job(setup_text_sensor_core_, sensor_var, mqtt_var, config)


def register_text_sensor(var, config):
    text_sensor_var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    rhs = App.register_text_sensor(text_sensor_var)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], rhs, has_side_effects=True)
    add_job(setup_text_sensor_core_, text_sensor_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_TEXT_SENSOR'
