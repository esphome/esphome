import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import CONF_API_CLIENT_ID, CONF_API_ENTITY_KEY, APIClient

DEPENDENCIES = ["api_client"]

text_sensor_ns = cg.esphome_ns.namespace("text_sensor")
TextSensor = text_sensor_ns.class_("TextSensor", text_sensor.TextSensor, cg.Nameable)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TextSensor),
        cv.GenerateID(CONF_API_CLIENT_ID): cv.use_id(APIClient),
        cv.Required(CONF_API_ENTITY_KEY): cv.uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_API_CLIENT_ID])
    var = cg.new_Pvariable(config[CONF_ID])

    yield text_sensor.register_text_sensor(var, config)

    cg.add(paren.register_text_sensor(config[CONF_API_ENTITY_KEY], var))
