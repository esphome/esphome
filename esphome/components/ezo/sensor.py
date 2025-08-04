from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID

from . import (
    CONF_ON_CALIBRATION,
    CONF_ON_CUSTOM,
    CONF_ON_DEVICE_INFORMATION,
    CONF_ON_LED,
    CONF_ON_SLOPE,
    CONF_ON_T,
    CalibrationTrigger,
    CustomTrigger,
    DeviceInformationTrigger,
    EZOSensor,
    LedTrigger,
    SlopeTrigger,
    TTrigger,
)

CODEOWNERS = ["@ssieb"]
DEPENDENCIES = ["ezo"]

CONFIG_SCHEMA = (
    sensor.sensor_schema(EZOSensor)
    .extend(
        {
            cv.Optional(CONF_ON_CUSTOM): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CustomTrigger),
                }
            ),
            cv.Optional(CONF_ON_CALIBRATION): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CalibrationTrigger),
                }
            ),
            cv.Optional(CONF_ON_SLOPE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SlopeTrigger),
                }
            ),
            cv.Optional(CONF_ON_T): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TTrigger),
                }
            ),
            cv.Optional(CONF_ON_DEVICE_INFORMATION): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        DeviceInformationTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_LED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LedTrigger),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(None))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)

    for conf in config.get(CONF_ON_CUSTOM, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_LED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(bool, "x")], conf)

    for conf in config.get(CONF_ON_DEVICE_INFORMATION, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_SLOPE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_CALIBRATION, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_T, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
