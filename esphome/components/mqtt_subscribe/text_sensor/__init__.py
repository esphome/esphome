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
            cv.Required(CONF_TOPIC): cv.templatable(cv.subscribe_topic),
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

    # Process topic - handle both static values and lambda expressions
    topic = config[CONF_TOPIC]
    if cg.is_template(topic):
        # Lambda expression - process and use lambda setter
        lambda_expr = await cg.process_lambda(topic, [], return_type=cg.std_string)
        cg.add(var.set_topic_lambda(lambda_expr))
    else:
        # Static value - use regular setter
        cg.add(var.set_topic(topic))

    cg.add(var.set_qos(config[CONF_QOS]))
