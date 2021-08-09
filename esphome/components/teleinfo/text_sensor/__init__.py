import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import teleinfo_ns, TeleInfo, CONF_TELEINFO_ID

CONF_TAG_NAME = "tag_name"

TeleInfoTextSensor = teleinfo_ns.class_(
    "TeleInfoTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TeleInfoTextSensor),
        cv.GenerateID(CONF_TELEINFO_ID): cv.use_id(TeleInfo),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    teleinfo = await cg.get_variable(config[CONF_TELEINFO_ID])
    cg.add(teleinfo.register_teleinfo_listener(var))
