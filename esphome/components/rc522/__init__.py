from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import nfc
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_FINISHED_WRITE,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_RESET_PIN,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@glmnet"]
AUTO_LOAD = ["binary_sensor", "nfc"]
MULTI_CONF = True

CONF_RC522_ID = "rc522_id"

rc522_ns = cg.esphome_ns.namespace("rc522")
RC522 = rc522_ns.class_("RC522", cg.PollingComponent)

RC522IsWritingCondition = rc522_ns.class_(
    "RC522IsWritingCondition", automation.Condition
)

RC522_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RC522),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
        cv.Optional(CONF_ON_FINISHED_WRITE): automation.validate_automation({}),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("1s"))


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FINISHED_WRITE, "add_on_finished_write_callback"
    ),
)


async def setup_rc522(var, config):
    await cg.register_component(var, config)

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x"), (nfc.NfcTag, "tag")], conf
        )

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontagremoved_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x"), (nfc.NfcTag, "tag")], conf
        )

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


@automation.register_condition(
    "rc522.is_writing",
    RC522IsWritingCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(RC522),
        }
    ),
)
async def rc522_is_writing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
