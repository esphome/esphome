import logging

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import (
    CONF_DLMS_METER_ID,
    DEPRECATED_TEXT_KEYS,
    DlmsMeterComponent,
    obis_code,
    warn_deprecated,
)

_LOGGER = logging.getLogger(__name__)
DEPENDENCIES = ["dlms_meter"]

CONF_OBIS_CODE = "obis_code"

# New single-entity platform schema
NEW_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)

# Old multi-entity (hub) platform schema for backwards compatibility
OLD_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
            cv.Optional("timestamp"): text_sensor.text_sensor_schema(),
            cv.Optional("meternumber"): text_sensor.text_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    warn_deprecated,
)

# Accepts both during the grace period
CONFIG_SCHEMA = cv.Any(NEW_SCHEMA, OLD_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])

    # Check if the user is using the new format
    if CONF_OBIS_CODE in config:
        var = await text_sensor.new_text_sensor(config)
        cg.add(hub.register_text_sensor(config[CONF_OBIS_CODE], var))
    else:
        # Transparently handle the deprecated format
        for key, obis_val in DEPRECATED_TEXT_KEYS.items():
            if key in config:
                sens = await text_sensor.new_text_sensor(config[key])
                cg.add(hub.register_text_sensor(obis_val, sens))
