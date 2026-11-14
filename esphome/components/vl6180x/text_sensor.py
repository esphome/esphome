"""Text sensor platform for VL6180X hubs."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_VL6180X_ID, DOMAIN, VL6180XComponent

DEPENDENCIES = ["vl6180x"]

CONF_RANGE_STATUS = "range_status"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_VL6180X_ID): cv.use_id(VL6180XComponent),
            cv.Optional(CONF_RANGE_STATUS): text_sensor.text_sensor_schema(
                icon="mdi:map-marker-distance",
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_RANGE_STATUS),
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=text_sensor.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_VL6180X_ID,),
    message="Only one VL6180X text_sensor platform entry is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_VL6180X_ID])
    sensors = text_sensor.sub_text_sensors(config)
    await sensors(CONF_RANGE_STATUS, hub.set_range_status_sensor)
