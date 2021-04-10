import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@ssieb"]

DEPENDENCIES = ["i2c"]

CONF_ON_LED = "on_led"

ezo_ns = cg.esphome_ns.namespace("ezo")

EZOSensor = ezo_ns.class_(
    "EZOSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

LedTrigger = ezo_ns.class_("LedTrigger", automation.Trigger.template(cg.bool_))


CONFIG_SCHEMA = (
    sensor.SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EZOSensor),
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

    for conf in config.get(CONF_ON_LED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(bool, "x")], conf)
