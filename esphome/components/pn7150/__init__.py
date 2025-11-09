from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import nfc
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IRQ_PIN,
    CONF_MESSAGE,
    CONF_ON_FINISHED_WRITE,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["binary_sensor", "nfc"]
CODEOWNERS = ["@kbx81", "@jesserockz"]

CONF_EMULATION_MESSAGE = "emulation_message"
CONF_EMULATION_OFF = "emulation_off"
CONF_EMULATION_ON = "emulation_on"
CONF_INCLUDE_ANDROID_APP_RECORD = "include_android_app_record"
CONF_ON_EMULATED_TAG_SCAN = "on_emulated_tag_scan"
CONF_PN7150_ID = "pn7150_id"
CONF_POLLING_OFF = "polling_off"
CONF_POLLING_ON = "polling_on"
CONF_SET_CLEAN_MODE = "set_clean_mode"
CONF_SET_EMULATION_MESSAGE = "set_emulation_message"
CONF_SET_FORMAT_MODE = "set_format_mode"
CONF_SET_READ_MODE = "set_read_mode"
CONF_SET_WRITE_MESSAGE = "set_write_message"
CONF_SET_WRITE_MODE = "set_write_mode"
CONF_TAG_TTL = "tag_ttl"
CONF_VEN_PIN = "ven_pin"

pn7150_ns = cg.esphome_ns.namespace("pn7150")
PN7150 = pn7150_ns.class_("PN7150", nfc.Nfcc, cg.Component)

EmulationOffAction = pn7150_ns.class_("EmulationOffAction", automation.Action)
EmulationOnAction = pn7150_ns.class_("EmulationOnAction", automation.Action)
PollingOffAction = pn7150_ns.class_("PollingOffAction", automation.Action)
PollingOnAction = pn7150_ns.class_("PollingOnAction", automation.Action)
SetCleanModeAction = pn7150_ns.class_("SetCleanModeAction", automation.Action)
SetEmulationMessageAction = pn7150_ns.class_(
    "SetEmulationMessageAction", automation.Action
)
SetFormatModeAction = pn7150_ns.class_("SetFormatModeAction", automation.Action)
SetReadModeAction = pn7150_ns.class_("SetReadModeAction", automation.Action)
SetWriteMessageAction = pn7150_ns.class_("SetWriteMessageAction", automation.Action)
SetWriteModeAction = pn7150_ns.class_("SetWriteModeAction", automation.Action)


PN7150OnEmulatedTagScanTrigger = pn7150_ns.class_(
    "PN7150OnEmulatedTagScanTrigger", automation.Trigger.template()
)

PN7150OnFinishedWriteTrigger = pn7150_ns.class_(
    "PN7150OnFinishedWriteTrigger", automation.Trigger.template()
)

PN7150IsWritingCondition = pn7150_ns.class_(
    "PN7150IsWritingCondition", automation.Condition
)


IsWritingCondition = nfc.nfc_ns.class_("IsWritingCondition", automation.Condition)


SIMPLE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(PN7150),
    }
)

SET_MESSAGE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(PN7150),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
        cv.Optional(CONF_INCLUDE_ANDROID_APP_RECORD, default=True): cv.boolean,
    }
)

PN7150_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PN7150),
        cv.Optional(CONF_ON_EMULATED_TAG_SCAN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    PN7150OnEmulatedTagScanTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_FINISHED_WRITE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    PN7150OnFinishedWriteTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
        cv.Required(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_VEN_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_EMULATION_MESSAGE): cv.string,
        cv.Optional(CONF_TAG_TTL): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


@automation.register_action(
    "tag.set_emulation_message",
    SetEmulationMessageAction,
    SET_MESSAGE_ACTION_SCHEMA,
)
@automation.register_action(
    "tag.set_write_message",
    SetWriteMessageAction,
    SET_MESSAGE_ACTION_SCHEMA,
)
async def pn7150_set_message_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    template_ = await cg.templatable(
        config[CONF_INCLUDE_ANDROID_APP_RECORD], args, cg.bool_
    )
    cg.add(var.set_include_android_app_record(template_))
    return var


@automation.register_action(
    "tag.emulation_off", EmulationOffAction, SIMPLE_ACTION_SCHEMA
)
@automation.register_action("tag.emulation_on", EmulationOnAction, SIMPLE_ACTION_SCHEMA)
@automation.register_action("tag.polling_off", PollingOffAction, SIMPLE_ACTION_SCHEMA)
@automation.register_action("tag.polling_on", PollingOnAction, SIMPLE_ACTION_SCHEMA)
@automation.register_action(
    "tag.set_clean_mode", SetCleanModeAction, SIMPLE_ACTION_SCHEMA
)
@automation.register_action(
    "tag.set_format_mode", SetFormatModeAction, SIMPLE_ACTION_SCHEMA
)
@automation.register_action(
    "tag.set_read_mode", SetReadModeAction, SIMPLE_ACTION_SCHEMA
)
@automation.register_action(
    "tag.set_write_mode", SetWriteModeAction, SIMPLE_ACTION_SCHEMA
)
async def pn7150_simple_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def setup_pn7150(var, config):
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(pin))

    pin = await cg.gpio_pin_expression(config[CONF_VEN_PIN])
    cg.add(var.set_ven_pin(pin))

    if emulation_message_config := config.get(CONF_EMULATION_MESSAGE):
        cg.add(var.set_tag_emulation_message(emulation_message_config))
        cg.add(var.set_tag_emulation_on())

    if CONF_TAG_TTL in config:
        cg.add(var.set_tag_ttl(config[CONF_TAG_TTL]))

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

    for conf in config.get(CONF_ON_EMULATED_TAG_SCAN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_FINISHED_WRITE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


@automation.register_condition(
    "pn7150.is_writing",
    PN7150IsWritingCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PN7150),
        }
    ),
)
async def pn7150_is_writing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
