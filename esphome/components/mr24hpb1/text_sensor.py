import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import CONF_MR24HPB1_ID, MR24HPB1Component

DEPENDENCIES = ["mr24hpb1"]

CONF_DEVICE_ID = "device_id"
CONF_SOFTWARE_VERSION = "software_version"
CONF_HARDWARE_VERSION = "hardware_version"
CONF_PROTOCOL_VERSION = "protocol_version"

# Enviroment status
CONF_ENVIRONMENT_STATUS = "environment_status"

# Movment type text sensor
CONF_MOVEMENT_TYPE = "movement_type"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPB1_ID): cv.use_id(MR24HPB1Component),
        cv.Optional(CONF_DEVICE_ID): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_HARDWARE_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_PROTOCOL_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_ENVIRONMENT_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_MOVEMENT_TYPE): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_MR24HPB1_ID])

    if CONF_DEVICE_ID in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_ID])
        cg.add(var.set_device_id_sensor(sens))

    if CONF_SOFTWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SOFTWARE_VERSION])
        cg.add(var.set_software_version_sensor(sens))

    if CONF_HARDWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_HARDWARE_VERSION])
        cg.add(var.set_hardware_version_sensor(sens))

    if CONF_PROTOCOL_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PROTOCOL_VERSION])
        cg.add(var.set_protocol_version_sensor(sens))

    if CONF_ENVIRONMENT_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ENVIRONMENT_STATUS])
        cg.add(var.set_environment_status_sensor(sens))

    if CONF_MOVEMENT_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MOVEMENT_TYPE])
        cg.add(var.set_movement_type_sensor(sens))
