import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_FORMAT, CONF_ID

from .. import CONF_OBIS_CODE, CONF_SERVER_ID, CONF_Q3DA_ID, Q3DA, obis_code, q3da_ns

AUTO_LOAD = ["q3da"]

Q3DAType = q3da_ns.enum("Q3DAType")
Q3DA_TYPES = {
    "text": Q3DAType.Q3DA_OCTET,
    "bool": Q3DAType.Q3DA_BOOL,
    "int": Q3DAType.Q3DA_INT,
    "uint": Q3DAType.Q3DA_UINT,
    "hex": Q3DAType.Q3DA_HEX,
    "": Q3DAType.Q3DA_UNDEFINED,
}

Q3DATextSensor = q3da_ns.class_("Q3DATextSensor", text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Q3DATextSensor),
        cv.GenerateID(CONF_Q3DA_ID): cv.use_id(Q3DA),
        cv.Required(CONF_OBIS_CODE): obis_code,
        cv.Optional(CONF_SERVER_ID, default=""): cv.string,
        cv.Optional(CONF_FORMAT, default=""): cv.enum(Q3DA_TYPES, lower=True),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_SERVER_ID],
        config[CONF_OBIS_CODE],
        config[CONF_FORMAT],
    )
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    q3da = await cg.get_variable(config[CONF_Q3DA_ID])
    cg.add(q3da.register_sml_listener(var))
