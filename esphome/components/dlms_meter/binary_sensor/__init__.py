import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import (
    CONF_DLMS_METER_ID,
    CONF_OBIS_CODE,
    DlmsMeterComponent,
    obis_string_to_byte_list,
    to_obis_id_struct,
)

DEPENDENCIES = ["dlms_meter"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_string_to_byte_list,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(hub.register_binary_sensor(to_obis_id_struct(config[CONF_OBIS_CODE]), var))
