import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    CONF_ID,
    UNIT_VOLT,
)

from . import EzoPMP


DEPENDENCIES = ["ezo_pmp"]

CONF_CURRENT_VOLUME_DOSED = "current_volume_dosed"
CONF_TOTAL_VOLUME_DOSED = "total_volume_dosed"
CONF_ABSOLUTE_TOTAL_VOLUME_DOSED = "absolute_total_volume_dosed"
CONF_PUMP_VOLTAGE = "pump_voltage"
CONF_LAST_VOLUME_REQUESTED = "last_volume_requested"
CONF_MAX_FLOW_RATE = "max_flow_rate"

UNIT_MILILITER = "ml"
UNIT_MILILITERS_PER_MINUTE = "ml/min"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(EzoPMP),
        cv.Optional(CONF_CURRENT_VOLUME_DOSED): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILILITER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional(CONF_LAST_VOLUME_REQUESTED): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILILITER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional(CONF_MAX_FLOW_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILILITERS_PER_MINUTE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_NONE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_TOTAL_VOLUME_DOSED): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILILITER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_ABSOLUTE_TOTAL_VOLUME_DOSED): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILILITER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_PUMP_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_CURRENT_VOLUME_DOSED in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_VOLUME_DOSED])
        cg.add(parent.set_current_volume_dosed(sens))

    if CONF_LAST_VOLUME_REQUESTED in config:
        sens = await sensor.new_sensor(config[CONF_LAST_VOLUME_REQUESTED])
        cg.add(parent.set_last_volume_requested(sens))

    if CONF_TOTAL_VOLUME_DOSED in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_VOLUME_DOSED])
        cg.add(parent.set_total_volume_dosed(sens))

    if CONF_ABSOLUTE_TOTAL_VOLUME_DOSED in config:
        sens = await sensor.new_sensor(config[CONF_ABSOLUTE_TOTAL_VOLUME_DOSED])
        cg.add(parent.set_absolute_total_volume_dosed(sens))

    if CONF_PUMP_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_PUMP_VOLTAGE])
        cg.add(parent.set_pump_voltage(sens))

    if CONF_MAX_FLOW_RATE in config:
        sens = await sensor.new_sensor(config[CONF_MAX_FLOW_RATE])
        cg.add(parent.set_max_flow_rate(sens))
