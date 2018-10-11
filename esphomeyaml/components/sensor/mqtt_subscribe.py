import voluptuous as vol

from esphomeyaml.components import sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_QOS, CONF_TOPIC
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['mqtt']

MakeMQTTSubscribeSensor = Application.MakeMQTTSubscribeSensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMQTTSubscribeSensor),
    vol.Required(CONF_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_QOS): cv.mqtt_qos,
}))


def to_code(config):
    rhs = App.make_mqtt_subscribe_sensor(config[CONF_NAME], config[CONF_TOPIC])
    make = variable(config[CONF_MAKE_ID], rhs)

    if CONF_QOS in config:
        add(make.Psensor.set_qos(config[CONF_QOS]))

    sensor.setup_sensor(make.Psensor, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_SENSOR'
