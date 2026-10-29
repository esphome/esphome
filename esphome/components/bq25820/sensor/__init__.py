import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
)

from .. import CONF_BQ25820_ID, BQ25820BaseSchema, bq25820_ns

DEPENDENCIES = ["bq25820"]

CONF_VBAT = "vbat"
CONF_VSYS = "vsys"
CONF_VAC = "vac"
CONF_IBAT = "ibat"
CONF_IAC = "iac"
CONF_TEMP_IN_C = "temp_in_c"

BQ25820VbatSensor = bq25820_ns.class_("BQ25820VbatSensor", sensor.Sensor)
BQ25820VsysSensor = bq25820_ns.class_("BQ25820VsysSensor", sensor.Sensor)
BQ25820VacSensor = bq25820_ns.class_("BQ25820VacSensor", sensor.Sensor)
BQ25820IbatSensor = bq25820_ns.class_("BQ25820IbatSensor", sensor.Sensor)
BQ25820IacSensor = bq25820_ns.class_("BQ25820IacSensor", sensor.Sensor)
BQ25820TempSensor = bq25820_ns.class_("BQ25820TempSensor", sensor.Sensor)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VBAT): sensor.sensor_schema(
            BQ25820VbatSensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VSYS): sensor.sensor_schema(
            BQ25820VsysSensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VAC): sensor.sensor_schema(
            BQ25820VacSensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IBAT): sensor.sensor_schema(
            BQ25820IbatSensor,
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IAC): sensor.sensor_schema(
            BQ25820IacSensor,
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMP_IN_C): sensor.sensor_schema(
            BQ25820TempSensor,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(BQ25820BaseSchema)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25820_ID])
    for key in (CONF_VBAT, CONF_VSYS, CONF_VAC, CONF_IBAT, CONF_IAC, CONF_TEMP_IN_C):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(parent.add_listener(sens))
