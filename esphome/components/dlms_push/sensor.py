import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from . import DlmsPushComponent, obis_code

DEPENDENCIES = ["dlms_push"]

CONF_DLMS_PUSH_ID = "dlms_push_id"
CONF_OBIS_CODE = "obis_code"

CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_PUSH_ID): cv.use_id(DlmsPushComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


async def to_code(config):
    # Retrieve the hub component
    hub = await cg.get_variable(config[CONF_DLMS_PUSH_ID])

    # Create the standard ESPHome sensor
    var = await sensor.new_sensor(config)

    # Register the sensor with the hub using its OBIS code
    cg.add(hub.register_sensor(config[CONF_OBIS_CODE], var))
