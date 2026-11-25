import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TEMPERATURE, ICON_THERMOMETER
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
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_DC_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_AC_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_DC_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_LIMITER_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, icon=ICON_THERMOMETER
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SUN_GTIL2_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_AC_VOLTAGE, hub.set_ac_voltage_sensor)
    await sensors(CONF_DC_VOLTAGE, hub.set_dc_voltage_sensor)
    await sensors(CONF_AC_POWER, hub.set_ac_power_sensor)
    await sensors(CONF_DC_POWER, hub.set_dc_power_sensor)
    await sensors(CONF_LIMITER_POWER, hub.set_limiter_power_sensor)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
