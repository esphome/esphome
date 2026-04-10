from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_IRQ_PIN,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_RESET_PIN,
    CONF_TRIGGER_ID,
)

from esphome.components import nfc

CODEOWNERS = ["@JohnMcLear"]
AUTO_LOAD = ["nfc"]
MULTI_CONF = True

CONF_ST25R_ID = "st25r_id"
CONF_RF_POWER = "rf_power"

st25r_ns = cg.esphome_ns.namespace("st25r")
ST25R = st25r_ns.class_("ST25R", nfc.Nfcc, cg.PollingComponent)

ST25RTagTrigger = st25r_ns.class_(
    "ST25RTagTrigger", automation.Trigger.template(cg.std_string)
)
ST25RTagRemovedTrigger = st25r_ns.class_(
    "ST25RTagRemovedTrigger", automation.Trigger.template(cg.std_string)
)

ST25R_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ST25R),
        cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RF_POWER, default=15): cv.int_range(min=0, max=15),
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ST25RTagTrigger),
            }
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ST25RTagRemovedTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("1s"))


async def setup_st25r(var, config):
    await cg.register_component(var, config)

    if CONF_IRQ_PIN in config:
        irq = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(var.set_irq_pin(irq))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    cg.add(var.set_rf_power(config[CONF_RF_POWER]))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(var.register_on_tag_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(var.register_on_tag_removed_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
