import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ICON_FLASH,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.types import ConfigType

from . import CONF_SUN_GTIL2_ID, SunGTIL2Component

CONF_AC_VOLTAGE = "ac_voltage"
CONF_DC_VOLTAGE = "dc_voltage"
CONF_AC_POWER = "ac_power"
CONF_DC_POWER = "dc_power"
CONF_LIMITER_POWER = "limiter_power"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_SUN_GTIL2_ID): cv.use_id(SunGTIL2Component),
            cv.Optional(CONF_AC_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DC_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AC_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DC_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LIMITER_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SUN_GTIL2_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_AC_VOLTAGE, hub.set_ac_voltage)
    await sensors(CONF_DC_VOLTAGE, hub.set_dc_voltage)
    await sensors(CONF_AC_POWER, hub.set_ac_power)
    await sensors(CONF_DC_POWER, hub.set_dc_power)
    await sensors(CONF_LIMITER_POWER, hub.set_limiter_power)
    await sensors(CONF_TEMPERATURE, hub.set_temperature)
