import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY
from . import CONF_API_CLIENT_ID, CONF_API_ENTITY_KEY, APIClient

DEPENDENCIES = ["api_client"]

sensor_ns = cg.esphome_ns.namespace("sensor")
Sensor = sensor_ns.class_("Sensor", sensor.Sensor, cg.Nameable)

CONFIG_SCHEMA = (
    sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 1)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(Sensor),
            cv.GenerateID(CONF_API_CLIENT_ID): cv.use_id(APIClient),
            cv.Required(CONF_API_ENTITY_KEY): cv.uint32_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_API_CLIENT_ID])
    var = cg.new_Pvariable(config[CONF_ID])

    yield sensor.register_sensor(var, config)

    cg.add(paren.register_sensor(config[CONF_API_ENTITY_KEY], var))
