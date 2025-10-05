import esphome.codegen as cg
from esphome.components import mqtt, sensor
import esphome.config_validation as cv
from esphome.const import CONF_QOS, CONF_TOPIC

from .. import mqtt_subscribe_ns

DEPENDENCIES = ["mqtt"]

CONF_MQTT_PARENT_ID = "mqtt_parent_id"
MQTTSubscribeSensor = mqtt_subscribe_ns.class_(
    "MQTTSubscribeSensor", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MQTTSubscribeSensor,
        accuracy_decimals=1,
    )
    .extend(
        {
            cv.GenerateID(CONF_MQTT_PARENT_ID): cv.use_id(mqtt.MQTTClientComponent),
            cv.Required(CONF_TOPIC): cv.subscribe_topic,
            cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MQTT_PARENT_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_topic(config[CONF_TOPIC]))
    cg.add(var.set_qos(config[CONF_QOS]))
