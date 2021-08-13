import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import CONF_API_CLIENT_ID, CONF_API_ENTITY_KEY, APIClient

DEPENDENCIES = ["api_client"]

binary_sensor_ns = cg.esphome_ns.namespace("binary_sensor")
BinarySensor = binary_sensor_ns.class_(
    "BinarySensor", binary_sensor.BinarySensor, cg.Nameable
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BinarySensor),
        cv.GenerateID(CONF_API_CLIENT_ID): cv.use_id(APIClient),
        cv.Required(CONF_API_ENTITY_KEY): cv.uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_API_CLIENT_ID])
    var = cg.new_Pvariable(config[CONF_ID])

    yield binary_sensor.register_binary_sensor(var, config)

    cg.add(paren.register_binary_sensor(config[CONF_API_ENTITY_KEY], var))
