import voluptuous as vol

from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_QOS, CONF_TOPIC
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['mqtt']

MQTTSubscribeTextSensor = text_sensor.text_sensor_ns.class_('MQTTSubscribeTextSensor',
                                                            text_sensor.TextSensor, Component)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeTextSensor),
    vol.Required(CONF_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_QOS): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_mqtt_subscribe_text_sensor(config[CONF_NAME], config[CONF_TOPIC])
    sensor_ = Pvariable(config[CONF_ID], rhs)

    if CONF_QOS in config:
        add(sensor_.set_qos(config[CONF_QOS]))

    text_sensor.setup_text_sensor(sensor_, config)
    setup_component(sensor_, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_TEXT_SENSOR'


def to_hass_config(data, config):
    return text_sensor.core_to_hass_config(data, config)
