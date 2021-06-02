import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, mqtt
from esphome.const import CONF_ID, CONF_QOS, CONF_TOPIC
from .. import mqtt_subscribe_ns

DEPENDENCIES = ["mqtt"]

CONF_MQTT_PARENT_ID = "mqtt_parent_id"
MQTTSubscribeTextSensor = mqtt_subscribe_ns.class_(
    "MQTTSubscribeTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MQTTSubscribeTextSensor),
        cv.GenerateID(CONF_MQTT_PARENT_ID): cv.use_id(mqtt.MQTTClientComponent),
        cv.Required(CONF_TOPIC): cv.subscribe_topic,
        cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    parent = await cg.get_variable(config[CONF_MQTT_PARENT_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_topic(config[CONF_TOPIC]))
    cg.add(var.set_qos(config[CONF_QOS]))
