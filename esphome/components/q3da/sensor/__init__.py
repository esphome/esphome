import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from .. import CONF_OBIS_CODE, CONF_SERVER_ID, CONF_Q3DA_ID, Q3DA, obis_code, q3da_ns

AUTO_LOAD = ["q3da"]

Q3DASensor = q3da_ns.class_("Q3DASensor", sensor.Sensor, cg.Component)


CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(): cv.declare_id(Q3DASensor),
        cv.GenerateID(CONF_Q3DA_ID): cv.use_id(Q3DA),
        cv.Required(CONF_OBIS_CODE): obis_code,
        cv.Optional(CONF_SERVER_ID, default=""): cv.string,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID], config[CONF_SERVER_ID], config[CONF_OBIS_CODE]
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    q3da = await cg.get_variable(config[CONF_Q3DA_ID])
    cg.add(q3da.register_q3da_listener(var))
