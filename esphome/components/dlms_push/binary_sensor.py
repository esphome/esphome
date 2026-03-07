import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DLMS_PUSH_ID, CONF_OBIS_CODE

from . import DlmsPushComponent, obis_code

DEPENDENCIES = ["dlms_push"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_PUSH_ID): cv.use_id(DlmsPushComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_PUSH_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(hub.register_binary_sensor(config[CONF_OBIS_CODE], var))
