import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_MR24HPC1_ID, MR24HPC1Component

CONF_HEART_BEAT = "heart_beat"
CONF_PRODUCT_MODEL = "product_model"
CONF_PRODUCT_ID = "product_id"
CONF_HARDWARE_MODEL = "hardware_model"
CONF_HARDWARE_VERSION = "hardware_version"

CONF_KEEP_AWAY = "keep_away"
CONF_MOTION_STATUS = "motion_status"

CONF_CUSTOM_MODE_END = "custom_mode_end"


# The entity category for read only diagnostic values, for example RSSI, uptime or MAC Address
CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(MR24HPC1Component),
    cv.Optional(CONF_HEART_BEAT): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:connection"
    ),
    cv.Optional(CONF_PRODUCT_MODEL): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:information-outline"
    ),
    cv.Optional(CONF_PRODUCT_ID): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:information-outline"
    ),
    cv.Optional(CONF_HARDWARE_MODEL): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:information-outline"
    ),
    cv.Optional(CONF_HARDWARE_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:information-outline"
    ),
    cv.Optional(CONF_KEEP_AWAY): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:walk"
    ),
    cv.Optional(CONF_MOTION_STATUS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:human-greeting"
    ),
    cv.Optional(CONF_CUSTOM_MODE_END): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:account-check"
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR24HPC1_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_HEART_BEAT, hub.set_heartbeat_state_text_sensor)
    await text_sensors(CONF_PRODUCT_MODEL, hub.set_product_model_text_sensor)
    await text_sensors(CONF_PRODUCT_ID, hub.set_product_id_text_sensor)
    await text_sensors(CONF_HARDWARE_MODEL, hub.set_hardware_model_text_sensor)
    await text_sensors(CONF_HARDWARE_VERSION, hub.set_firware_version_text_sensor)
    await text_sensors(CONF_KEEP_AWAY, hub.set_keep_away_text_sensor)
    await text_sensors(CONF_MOTION_STATUS, hub.set_motion_status_text_sensor)
    await text_sensors(CONF_CUSTOM_MODE_END, hub.set_custom_mode_end_text_sensor)
