import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_DLMS_METER_ID, CONF_OBIS_CODE, DlmsMeterComponent, obis_code

DEPENDENCIES = ["dlms_meter"]

# Removed in 2026.11.0 - kept to provide helpful error message
# Remove before 2027.5.0
REMOVED_KEYS = {
    "timestamp": "0.0.1.0.0.255",
    "meternumber": "0.0.96.1.0.255",
}


CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
        **{
            cv.Optional(key): cv.invalid(
                f"The predefined '{key}' key was removed in ESPHome 2026.11.0. "
                f"Add a separate '- platform: dlms_meter' text sensor with "
                f'obis_code: "{obis}" instead'
            )
            for key, obis in REMOVED_KEYS.items()
        },
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(hub.register_text_sensor(config[CONF_OBIS_CODE], var))
