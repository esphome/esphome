import voluptuous as vol

from esphomeyaml.components import sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_QOS, CONF_TOPIC
from esphomeyaml.helpers import App, Application, add, variable, setup_component, Component

DEPENDENCIES = ['mqtt']

MakeMQTTSubscribeSensor = Application.struct('MakeMQTTSubscribeSensor')
MQTTSubscribeSensor = sensor.sensor_ns.class_('MQTTSubscribeSensor', sensor.Sensor, Component)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMQTTSubscribeSensor),
    vol.Required(CONF_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_QOS): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_mqtt_subscribe_sensor(config[CONF_NAME], config[CONF_TOPIC])
    make = variable(config[CONF_MAKE_ID], rhs)
    subs = make.Psensor

    if CONF_QOS in config:
        add(subs.set_qos(config[CONF_QOS]))

    sensor.setup_sensor(subs, make.Pmqtt, config)
    setup_component(subs, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
