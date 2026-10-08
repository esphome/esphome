import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
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

UNIT_MILILITER = "mL"
UNIT_MILILITERS_PER_MINUTE = "mL/min"

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
    hub = await cg.get_variable(config[CONF_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CURRENT_VOLUME_DOSED, hub.set_current_volume_dosed)
    await sensors(CONF_LAST_VOLUME_REQUESTED, hub.set_last_volume_requested)
    await sensors(CONF_TOTAL_VOLUME_DOSED, hub.set_total_volume_dosed)
    await sensors(CONF_ABSOLUTE_TOTAL_VOLUME_DOSED, hub.set_absolute_total_volume_dosed)
    await sensors(CONF_PUMP_VOLTAGE, hub.set_pump_voltage)
    await sensors(CONF_MAX_FLOW_RATE, hub.set_max_flow_rate)
