import logging

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_DLMS_METER_ID, CONF_OBIS_CODE, DlmsMeterComponent, obis_code

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["dlms_meter"]

TEXT_KEYS = {
    "timestamp": "0.0.1.0.0.255",
    "meternumber": "0.0.96.1.0.255",
}

DYNAMIC_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


def deprecation_warning(config):
    _LOGGER.warning(
        "The dlms_meter text_sensor schema using predefined keys (e.g., 'timestamp') is deprecated and will be removed in 2026.11.0. "
        "Please update your configuration to use the new schema with 'obis_code'."
    )
    return config


OLD_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
            cv.Optional("timestamp"): text_sensor.text_sensor_schema(),
            cv.Optional("meternumber"): text_sensor.text_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    deprecation_warning,
)


CONFIG_SCHEMA = cv.Any(DYNAMIC_SCHEMA, OLD_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])

    if obis := config.get(CONF_OBIS_CODE):
        var = await text_sensor.new_text_sensor(config)
        cg.add(hub.register_text_sensor(obis, var))
    else:
        for key, obis_val in TEXT_KEYS.items():
            if text_sensor_config := config.get(key):
                sens = await text_sensor.new_text_sensor(text_sensor_config)
                cg.add(hub.register_text_sensor(obis_val, sens))
