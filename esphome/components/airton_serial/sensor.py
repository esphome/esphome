import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_HOUR,
)
from esphome.types import ConfigType

from .climate import CONF_AIRTON_SERIAL_ID, AirtonSerialClimate

CODEOWNERS = ["@guiguid"]

CONF_TOTAL_USAGE_TIME = "total_usage_time"
CONF_REVISION = "revision"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_SERIAL_ID): cv.use_id(AirtonSerialClimate),
        cv.Optional(CONF_TOTAL_USAGE_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_REVISION): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AIRTON_SERIAL_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TOTAL_USAGE_TIME, hub.set_total_usage_time_sensor)
    await sensors(CONF_REVISION, hub.set_revision_sensor)
