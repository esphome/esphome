import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CAPACITY,
    CONF_FINGERPRINT_COUNT,
    CONF_LAST_CONFIDENCE,
    CONF_LAST_FINGER_ID,
    CONF_SECURITY_LEVEL,
    CONF_STATUS,
    DEVICE_CLASS_EMPTY,
    ICON_ACCOUNT,
    ICON_ACCOUNT_CHECK,
    ICON_DATABASE,
    ICON_EMPTY,
    ICON_FINGERPRINT,
    ICON_SECURITY,
    STATE_CLASS_NONE,
    UNIT_EMPTY,
)
from . import CONF_FINGERPRINT_GROW_ID, FingerprintGrowComponent

DEPENDENCIES = ["fingerprint_grow"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGERPRINT_GROW_ID): cv.use_id(FingerprintGrowComponent),
        cv.Optional(CONF_FINGERPRINT_COUNT): sensor.sensor_schema(
            UNIT_EMPTY, ICON_FINGERPRINT, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional(CONF_STATUS): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional(CONF_CAPACITY): sensor.sensor_schema(
            UNIT_EMPTY, ICON_DATABASE, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional(CONF_SECURITY_LEVEL): sensor.sensor_schema(
            UNIT_EMPTY, ICON_SECURITY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional(CONF_LAST_FINGER_ID): sensor.sensor_schema(
            UNIT_EMPTY, ICON_ACCOUNT, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional(CONF_LAST_CONFIDENCE): sensor.sensor_schema(
            UNIT_EMPTY, ICON_ACCOUNT_CHECK, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FINGERPRINT_GROW_ID])

    for key in [
        CONF_FINGERPRINT_COUNT,
        CONF_STATUS,
        CONF_CAPACITY,
        CONF_SECURITY_LEVEL,
        CONF_LAST_FINGER_ID,
        CONF_LAST_CONFIDENCE,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))
