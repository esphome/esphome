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
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "nfc"]
CODEOWNERS = ["@kbx81", "@jesserockz"]

CONF_EMULATION_MESSAGE = "emulation_message"
CONF_INCLUDE_ANDROID_APP_RECORD = "include_android_app_record"
CONF_ON_EMULATED_TAG_SCAN = "on_emulated_tag_scan"
CONF_TAG_TTL = "tag_ttl"
CONF_VEN_PIN = "ven_pin"

pn71xx_ns = cg.esphome_ns.namespace("pn71xx")
PN71xx = pn71xx_ns.class_("PN71xx", nfc.Nfcc, cg.Component)

SIMPLE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(PN71xx),
    }
)

SET_MESSAGE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(PN71xx),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
        cv.Optional(CONF_INCLUDE_ANDROID_APP_RECORD, default=True): cv.boolean,
    }
)

PN71XX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_EMULATED_TAG_SCAN): automation.validate_automation({}),
        cv.Optional(CONF_ON_FINISHED_WRITE): automation.validate_automation({}),
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


for _name, _method in (
    ("tag.set_emulation_message", "set_tag_emulation_message"),
    ("tag.set_write_message", "set_tag_write_message"),
):
    automation.register_apply_action(
        _name,
        SET_MESSAGE_ACTION_SCHEMA,
        automation.ApplyCall(
            f"{_method}({{}}, {{}})",
            (
                (CONF_MESSAGE, cg.std_string),
                (CONF_INCLUDE_ANDROID_APP_RECORD, cg.bool_),
            ),
        ),
    )

for _name, _call in (
    ("tag.emulation_off", "set_tag_emulation_off()"),
    ("tag.emulation_on", "set_tag_emulation_on()"),
    ("tag.polling_off", "set_polling_off()"),
    ("tag.polling_on", "set_polling_on()"),
    ("tag.set_clean_mode", "clean_mode()"),
    ("tag.set_format_mode", "format_mode()"),
    ("tag.set_read_mode", "read_mode()"),
    ("tag.set_write_mode", "write_mode()"),
):
    automation.register_apply_action(
        _name, SIMPLE_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


def register_is_writing_condition(name: str, chip_class: MockObj) -> None:
    """Register the chip-specific ``<chip>.is_writing`` condition."""
    automation.register_apply_condition(
        name,
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(chip_class),
            }
        ),
        "is_writing()",
    )


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_EMULATED_TAG_SCAN, "add_on_emulated_tag_scan_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_FINISHED_WRITE, "add_on_finished_write_callback"
    ),
)


async def setup_pn71xx(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(pin))

    pin = await cg.gpio_pin_expression(config[CONF_VEN_PIN])
    cg.add(var.set_ven_pin(pin))

    if emulation_message_config := config.get(CONF_EMULATION_MESSAGE):
        cg.add(var.set_tag_emulation_message(emulation_message_config))
        cg.add(var.set_tag_emulation_on())

    if (tag_ttl := config.get(CONF_TAG_TTL)) is not None:
        cg.add(var.set_tag_ttl(tag_ttl))

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
