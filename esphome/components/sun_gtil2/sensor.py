import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ICON_FLASH,
    UNIT_VOLT,
    ICON_THERMOMETER,
    UNIT_WATT,
    UNIT_CELSIUS,
    CONF_TEMPERATURE,
)
from . import SunGTIL2Component, CONF_SUN_GTIL2_ID

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
            ),
            cv.Optional(CONF_DC_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
            ),
            cv.Optional(CONF_AC_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
            ),
            cv.Optional(CONF_DC_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
            ),
            cv.Optional(CONF_LIMITER_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SUN_GTIL2_ID])
    if ac_voltage_config := config.get(CONF_AC_VOLTAGE):
        sens = await sensor.new_sensor(ac_voltage_config)
        cg.add(hub.set_ac_voltage(sens))
    if dc_voltage_config := config.get(CONF_DC_VOLTAGE):
        sens = await sensor.new_sensor(dc_voltage_config)
        cg.add(hub.set_dc_voltage(sens))
    if ac_power_config := config.get(CONF_AC_POWER):
        sens = await sensor.new_sensor(ac_power_config)
        cg.add(hub.set_ac_power(sens))
    if dc_power_config := config.get(CONF_DC_POWER):
        sens = await sensor.new_sensor(dc_power_config)
        cg.add(hub.set_dc_power(sens))
    if limiter_power_config := config.get(CONF_LIMITER_POWER):
        sens = await sensor.new_sensor(limiter_power_config)
        cg.add(hub.set_limiter_power(sens))
    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(hub.set_temperature(sens))
