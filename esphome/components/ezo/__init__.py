from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv

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

CONFIG_SCHEMA = cv.Schema({}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_global(cg.RawStatement('#include "esphome/components/ezo/ezo.h"'))
