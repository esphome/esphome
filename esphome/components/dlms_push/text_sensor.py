import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DLMS_PUSH_ID, CONF_OBIS_CODE

from . import DlmsPushComponent, obis_code

DEPENDENCIES = ["dlms_push"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_PUSH_ID): cv.use_id(DlmsPushComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_PUSH_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(hub.register_text_sensor(config[CONF_OBIS_CODE], var))
