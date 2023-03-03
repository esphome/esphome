import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import microphone, binary_sensor
from esphome.const import CONF_ID, CONF_BINARY_SENSOR

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["api", "microphone", "binary_sensor", "esp32"]

push_to_talk_ns = cg.esphome_ns.namespace("push_to_talk")
PushToTalk = push_to_talk_ns.class_("PushToTalk", cg.PollingComponent)

CONF_MICROPHONE = "microphone"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PushToTalk),
        cv.GenerateID(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
        cv.Required(CONF_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic_ = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic_))

    binary_sensor_ = await cg.get_variable(config[CONF_BINARY_SENSOR])
    cg.add(var.set_binary_sensor(binary_sensor_))

    cg.add_define("USE_PUSH_TO_TALK")
