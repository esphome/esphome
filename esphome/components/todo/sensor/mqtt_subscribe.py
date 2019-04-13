from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_QOS, CONF_TOPIC


DEPENDENCIES = ['mqtt']

MQTTSubscribeSensor = sensor.sensor_ns.class_('MQTTSubscribeSensor', sensor.Sensor, Component)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeSensor),
    cv.Required(CONF_TOPIC): cv.subscribe_topic,
    cv.Optional(CONF_QOS): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    rhs = App.make_mqtt_subscribe_sensor(config[CONF_NAME], config[CONF_TOPIC])
    subs = Pvariable(config[CONF_ID], rhs)

    if CONF_QOS in config:
        cg.add(subs.set_qos(config[CONF_QOS]))

    sensor.setup_sensor(subs, config)
    register_component(subs, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_SENSOR'
