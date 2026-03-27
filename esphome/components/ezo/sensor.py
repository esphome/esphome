from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

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

CONFIG_SCHEMA = (
    sensor.sensor_schema(EZOSensor)
    .extend(
        {
            cv.Optional(CONF_ON_CUSTOM): automation.validate_automation({}),
            cv.Optional(CONF_ON_CALIBRATION): automation.validate_automation({}),
            cv.Optional(CONF_ON_SLOPE): automation.validate_automation({}),
            cv.Optional(CONF_ON_T): automation.validate_automation({}),
            cv.Optional(CONF_ON_DEVICE_INFORMATION): automation.validate_automation({}),
            cv.Optional(CONF_ON_LED): automation.validate_automation({}),
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
        await automation.build_callback_automation(
            var, "add_custom_callback", [(cg.std_string, "x")], conf
        )
    for conf in config.get(CONF_ON_LED, []):
        await automation.build_callback_automation(
            var, "add_led_state_callback", [(bool, "x")], conf
        )
    for conf in config.get(CONF_ON_DEVICE_INFORMATION, []):
        await automation.build_callback_automation(
            var, "add_device_infomation_callback", [(cg.std_string, "x")], conf
        )
    for conf in config.get(CONF_ON_SLOPE, []):
        await automation.build_callback_automation(
            var, "add_slope_callback", [(cg.std_string, "x")], conf
        )
    for conf in config.get(CONF_ON_CALIBRATION, []):
        await automation.build_callback_automation(
            var, "add_calibration_callback", [(cg.std_string, "x")], conf
        )
    for conf in config.get(CONF_ON_T, []):
        await automation.build_callback_automation(
            var, "add_t_callback", [(cg.std_string, "x")], conf
        )
