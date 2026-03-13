import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import CONF_DLMS_METER_ID, DlmsMeterComponent

DEPENDENCIES = ["dlms_meter"]

CONF_OBIS_CODE = "obis_code"

CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): cv.string,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await sensor.new_sensor(config)
    cg.add(hub.register_sensor(config[CONF_OBIS_CODE], var))
