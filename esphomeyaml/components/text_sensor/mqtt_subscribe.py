import voluptuous as vol

from esphomeyaml.components import text_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_QOS, CONF_TOPIC
from esphomeyaml.helpers import App, Application, add, variable, setup_component, Component

DEPENDENCIES = ['mqtt']

MakeMQTTSubscribeTextSensor = Application.struct('MakeMQTTSubscribeTextSensor')
MQTTSubscribeTextSensor = text_sensor.text_sensor_ns.class_('MQTTSubscribeTextSensor',
                                                            text_sensor.TextSensor, Component)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeTextSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMQTTSubscribeTextSensor),
    vol.Required(CONF_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_QOS): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_mqtt_subscribe_text_sensor(config[CONF_NAME], config[CONF_TOPIC])
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor_ = make.Psensor

    if CONF_QOS in config:
        add(sensor_.set_qos(config[CONF_QOS]))

    text_sensor.setup_text_sensor(sensor_, make.Pmqtt, config)
    setup_component(sensor_, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_TEXT_SENSOR'


def to_hass_config(data, config):
    return text_sensor.core_to_hass_config(data, config)
