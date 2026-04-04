import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import (
    CONF_DLMS_METER_ID,
    CONF_OBIS_CODE,
    TEXT_KEYS,
    DlmsMeterComponent,
    obis_code,
)

DEPENDENCIES = ["dlms_meter"]

DYNAMIC_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)

OLD_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
            cv.Optional("timestamp"): text_sensor.text_sensor_schema(),
            cv.Optional("meternumber"): text_sensor.text_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)

CONFIG_SCHEMA = cv.Any(DYNAMIC_SCHEMA, OLD_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])

    if CONF_OBIS_CODE in config:
        var = await text_sensor.new_text_sensor(config)
        cg.add(hub.register_text_sensor(config[CONF_OBIS_CODE], var))
    else:
        for key, obis_val in TEXT_KEYS.items():
            if key in config:
                sens = await text_sensor.new_text_sensor(config[key])
                cg.add(hub.register_text_sensor(obis_val, sens))
