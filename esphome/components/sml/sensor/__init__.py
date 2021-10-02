import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from .. import CONF_OBIS, CONF_SERVER_ID, CONF_SML_ID, Sml, obis, sml_ns

SmlSensor = sml_ns.class_("SmlSensor", sensor.Sensor, cg.Component)


CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(): cv.declare_id(SmlSensor),
        cv.GenerateID(CONF_SML_ID): cv.use_id(Sml),
        cv.Required(CONF_OBIS): obis,
        cv.Optional(CONF_SERVER_ID, default=""): cv.string,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_SERVER_ID], config[CONF_OBIS])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    sml = await cg.get_variable(config[CONF_SML_ID])
    cg.add(sml.register_sml_listener(var))
