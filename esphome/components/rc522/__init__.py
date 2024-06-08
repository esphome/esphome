import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import i2c
from esphome.const import (
    CONF_GAIN,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_TRIGGER_ID,
    CONF_RESET_PIN,
)

CODEOWNERS = ["@glmnet"]
AUTO_LOAD = ["binary_sensor"]

CONF_RC522_ID = "rc522_id"

rc522_ns = cg.esphome_ns.namespace("rc522")
RC522 = rc522_ns.class_("RC522", cg.PollingComponent, i2c.I2CDevice)
RC522Trigger = rc522_ns.class_(
    "RC522Trigger", automation.Trigger.template(cg.std_string)
)

RC522Gain = rc522_ns.enum("RC522Gain")
GAIN = {
    "18dB": RC522Gain.RC522_GAIN_18DB,
    "23dB": RC522Gain.RC522_GAIN_23DB,
    "18dB_a": RC522Gain.RC522_GAIN_18DBA,
    "23dB_a": RC522Gain.RC522_GAIN_23DBA,
    "33dB": RC522Gain.RC522_GAIN_33DB,
    "38dB": RC522Gain.RC522_GAIN_38DB,
    "43dB": RC522Gain.RC522_GAIN_43DB,
    "48dB": RC522Gain.RC522_GAIN_48DB,
}

RC522_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RC522),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RC522Trigger),
            }
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RC522Trigger),
            }
        ),
        cv.Optional(CONF_GAIN, default="38dB"): cv.decibel,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_rc522(var, config):
    await cg.register_component(var, config)

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontagremoved_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    cg.add(var.set_gain(config[CONF_GAIN]))
