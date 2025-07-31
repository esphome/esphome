import esphome.codegen as cg
from esphome.components import mqtt, text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_QOS, CONF_TOPIC

from .. import mqtt_subscribe_ns

DEPENDENCIES = ["mqtt"]

CONF_MQTT_PARENT_ID = "mqtt_parent_id"
MQTTSubscribeTextSensor = mqtt_subscribe_ns.class_(
    "MQTTSubscribeTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.GenerateID(): cv.declare_id(MQTTSubscribeTextSensor),
            cv.GenerateID(CONF_MQTT_PARENT_ID): cv.use_id(mqtt.MQTTClientComponent),
            cv.Required(CONF_TOPIC): cv.subscribe_topic,
            cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MQTT_PARENT_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_topic(config[CONF_TOPIC]))
    cg.add(var.set_qos(config[CONF_QOS]))
