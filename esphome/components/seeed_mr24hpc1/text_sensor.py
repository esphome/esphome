import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

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


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if heartbeat_config := config.get(CONF_HEART_BEAT):
        sens = await text_sensor.new_text_sensor(heartbeat_config)
        cg.add(mr24hpc1_component.set_heartbeat_state_text_sensor(sens))
    if productmodel_config := config.get(CONF_PRODUCT_MODEL):
        sens = await text_sensor.new_text_sensor(productmodel_config)
        cg.add(mr24hpc1_component.set_product_model_text_sensor(sens))
    if productid_config := config.get(CONF_PRODUCT_ID):
        sens = await text_sensor.new_text_sensor(productid_config)
        cg.add(mr24hpc1_component.set_product_id_text_sensor(sens))
    if hardwaremodel_config := config.get(CONF_HARDWARE_MODEL):
        sens = await text_sensor.new_text_sensor(hardwaremodel_config)
        cg.add(mr24hpc1_component.set_hardware_model_text_sensor(sens))
    if firwareversion_config := config.get(CONF_HARDWARE_VERSION):
        sens = await text_sensor.new_text_sensor(firwareversion_config)
        cg.add(mr24hpc1_component.set_firware_version_text_sensor(sens))
    if keepaway_config := config.get(CONF_KEEP_AWAY):
        sens = await text_sensor.new_text_sensor(keepaway_config)
        cg.add(mr24hpc1_component.set_keep_away_text_sensor(sens))
    if motionstatus_config := config.get(CONF_MOTION_STATUS):
        sens = await text_sensor.new_text_sensor(motionstatus_config)
        cg.add(mr24hpc1_component.set_motion_status_text_sensor(sens))
    if custommodeend_config := config.get(CONF_CUSTOM_MODE_END):
        sens = await text_sensor.new_text_sensor(custommodeend_config)
        cg.add(mr24hpc1_component.set_custom_mode_end_text_sensor(sens))
