from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_QOS, CONF_TOPIC


DEPENDENCIES = ['mqtt']

MQTTSubscribeTextSensor = text_sensor.text_sensor_ns.class_('MQTTSubscribeTextSensor',
                                                            text_sensor.TextSensor, Component)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeTextSensor),
    cv.Required(CONF_TOPIC): cv.subscribe_topic,
    cv.Optional(CONF_QOS): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    rhs = App.make_mqtt_subscribe_text_sensor(config[CONF_NAME], config[CONF_TOPIC])
    sensor_ = Pvariable(config[CONF_ID], rhs)

    if CONF_QOS in config:
        cg.add(sensor_.set_qos(config[CONF_QOS]))

    text_sensor.setup_text_sensor(sensor_, config)
    register_component(sensor_, config)


BUILD_FLAGS = '-DUSE_MQTT_SUBSCRIBE_TEXT_SENSOR'
