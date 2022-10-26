import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@ssieb"]

DEPENDENCIES = ["i2c"]

CONF_ON_LED = "on_led"
CONF_ON_DEVICE_INFORMATION = "on_device_information"
CONF_ON_SLOPE = "on_slope"
CONF_ON_CALIBRATION = "on_calibration"
CONF_ON_T = "on_t"
CONF_ON_CUSTOM = "on_custom"

ezo_ns = cg.esphome_ns.namespace("ezo")

EZOSensor = ezo_ns.class_(
    "EZOSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CustomTrigger = ezo_ns.class_(
    "CustomTrigger", automation.Trigger.template(cg.std_string)
)


TTrigger = ezo_ns.class_("TTrigger", automation.Trigger.template(cg.std_string))

SlopeTrigger = ezo_ns.class_("SlopeTrigger", automation.Trigger.template(cg.std_string))

CalibrationTrigger = ezo_ns.class_(
    "CalibrationTrigger", automation.Trigger.template(cg.std_string)
)

DeviceInformationTrigger = ezo_ns.class_(
    "DeviceInformationTrigger", automation.Trigger.template(cg.std_string)
)

LedTrigger = ezo_ns.class_("LedTrigger", automation.Trigger.template(cg.bool_))

CONFIG_SCHEMA = (
    sensor.SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EZOSensor),
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
