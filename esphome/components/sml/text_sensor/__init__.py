import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_FORMAT, CONF_ID

from .. import CONF_OBIS_CODE, CONF_SERVER_ID, CONF_SML_ID, Sml, obis_code, sml_ns

AUTO_LOAD = ["sml"]

SmlType = sml_ns.enum("SmlType")
SML_TYPES = {
    "text": SmlType.SML_OCTET,
    "bool": SmlType.SML_BOOL,
    "int": SmlType.SML_INT,
    "uint": SmlType.SML_UINT,
    "hex": SmlType.SML_HEX,
    "": SmlType.SML_UNDEFINED,
}

SmlTextSensor = sml_ns.class_("SmlTextSensor", text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SmlTextSensor),
        cv.GenerateID(CONF_SML_ID): cv.use_id(Sml),
        cv.Required(CONF_OBIS_CODE): obis_code,
        cv.Optional(CONF_SERVER_ID, default=""): cv.string,
        cv.Optional(CONF_FORMAT, default=""): cv.enum(SML_TYPES, lower=True),
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
    sml = await cg.get_variable(config[CONF_SML_ID])
    cg.add(sml.register_sml_listener(var))
