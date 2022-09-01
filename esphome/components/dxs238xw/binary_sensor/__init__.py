from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import (
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_POWER,
)

from .. import CONF_DXS238XW_ID, DXS238XW_COMPONENT_SCHEMA

DEPENDENCIES = ["dxs238xw"]

CONF_WARNING_OFF_BY_OVER_VOLTAGE = "warning_off_by_over_voltage"
CONF_WARNING_OFF_BY_UNDER_VOLTAGE = "warning_off_by_under_voltage"
CONF_WARNING_OFF_BY_OVER_CURRENT = "warning_off_by_over_current"
CONF_WARNING_OFF_BY_END_PURCHASE = "warning_off_by_end_purchase"
CONF_WARNING_OFF_BY_END_DELAY = "warning_off_by_end_delay"
CONF_WARNING_OFF_BY_USER = "warning_off_by_user"
CONF_WARNING_PURCHASE_ALARM = "warning_purchase_alarm"
CONF_METER_STATE = "meter_state"


TYPES = {
    CONF_WARNING_OFF_BY_OVER_VOLTAGE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_WARNING_OFF_BY_UNDER_VOLTAGE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_WARNING_OFF_BY_OVER_CURRENT: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_WARNING_OFF_BY_END_PURCHASE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_WARNING_OFF_BY_END_DELAY: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_WARNING_OFF_BY_USER: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_WARNING_PURCHASE_ALARM: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_METER_STATE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_POWER,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = DXS238XW_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, schema in TYPES.items()}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_DXS238XW_ID])

    for type, _ in TYPES.items():
        if type in config:
            conf = config[type]
            var = await binary_sensor.new_binary_sensor(conf)
            cg.add(getattr(paren, f"set_{type}_binary_sensor")(var))
