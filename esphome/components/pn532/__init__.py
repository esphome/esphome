from esphome import automation
import esphome.codegen as cg
from esphome.components import nfc
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_FINISHED_WRITE,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@OttoWinter", "@jesserockz"]
AUTO_LOAD = ["binary_sensor", "nfc"]
MULTI_CONF = True

CONF_PN532_ID = "pn532_id"

pn532_ns = cg.esphome_ns.namespace("pn532")
PN532 = pn532_ns.class_("PN532", cg.PollingComponent)

PN532OnFinishedWriteTrigger = pn532_ns.class_(
    "PN532OnFinishedWriteTrigger", automation.Trigger.template()
)

PN532IsWritingCondition = pn532_ns.class_(
    "PN532IsWritingCondition", automation.Condition
)

PN532_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PN532),
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
        cv.Optional(CONF_ON_FINISHED_WRITE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    PN532OnFinishedWriteTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("1s"))


def CONFIG_SCHEMA(conf):
    if conf:
        raise cv.Invalid(
            "This component has been moved in 1.16, please see the docs for updated "
            "instructions. https://esphome.io/components/binary_sensor/pn532.html"
        )


async def setup_pn532(var, config):
    await cg.register_component(var, config)

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

    for conf in config.get(CONF_ON_FINISHED_WRITE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


@automation.register_condition(
    "pn532.is_writing",
    PN532IsWritingCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PN532),
        }
    ),
)
async def pn532_is_writing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
