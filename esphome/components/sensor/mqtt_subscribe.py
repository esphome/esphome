import voluptuous as vol

from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_QOS, CONF_TOPIC
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['mqtt']

MQTTSubscribeSensor = sensor.sensor_ns.class_('MQTTSubscribeSensor', sensor.Sensor, Component)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeSensor),
    vol.Required(CONF_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_QOS): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_mqtt_subscribe_sensor(config[CONF_NAME], config[CONF_TOPIC])
    subs = Pvariable(config[CONF_ID], rhs)

    if CONF_QOS in config:
        add(subs.set_qos(config[CONF_QOS]))

    sensor.setup_sensor(subs, config)
    setup_component(subs, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
