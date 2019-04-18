import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mqtt, sensor
from esphome.const import CONF_ID, CONF_QOS, CONF_TOPIC
from .. import mqtt_subscribe_ns

DEPENDENCIES = ['mqtt']

CONF_MQTT_PARENT_ID = 'mqtt_parent_id'
MQTTSubscribeSensor = mqtt_subscribe_ns.class_('MQTTSubscribeSensor', sensor.Sensor, cg.Component)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MQTTSubscribeSensor),
    cv.GenerateID(CONF_MQTT_PARENT_ID): cv.use_variable_id(mqtt.MQTTClientComponent),
    cv.Required(CONF_TOPIC): cv.subscribe_topic,
    cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    parent = yield cg.get_variable(config[CONF_MQTT_PARENT_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_topic(config[CONF_TOPIC]))
    cg.add(var.set_qos(config[CONF_QOS]))
