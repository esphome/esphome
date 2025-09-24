import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_OBIS_CODE, CONF_SERVER_ID, CONF_SML_ID, Sml, obis_code, sml_ns

AUTO_LOAD = ["sml"]

SmlSensor = sml_ns.class_("SmlSensor", sensor.Sensor, cg.Component)


CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(): cv.declare_id(SmlSensor),
        cv.GenerateID(CONF_SML_ID): cv.use_id(Sml),
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
    sml = await cg.get_variable(config[CONF_SML_ID])
    cg.add(sml.register_sml_listener(var))
