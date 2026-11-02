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
from esphome.types import ConfigType

from .. import CONF_BQ25820_ID, BQ25820BaseSchema

DEPENDENCIES = ["bq25820"]

CONF_VBAT = "vbat"
CONF_VSYS = "vsys"
CONF_VAC = "vac"
CONF_IBAT = "ibat"
CONF_IAC = "iac"
CONF_TEMP_IN_C = "temp_in_c"


_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VBAT): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_VSYS): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_VAC): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_IBAT): _CURRENT_SCHEMA,
        cv.Optional(CONF_IAC): _CURRENT_SCHEMA,
        cv.Optional(CONF_TEMP_IN_C): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(BQ25820BaseSchema)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VBAT, hub.set_vbat_sensor)
    await sensors(CONF_VSYS, hub.set_vsys_sensor)
    await sensors(CONF_VAC, hub.set_vac_sensor)
    await sensors(CONF_IBAT, hub.set_ibat_sensor)
    await sensors(CONF_IAC, hub.set_iac_sensor)
    await sensors(CONF_TEMP_IN_C, hub.set_temp_in_c_sensor)
