from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import key_provider
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_KEY, CONF_ON_TAG, CONF_TRIGGER_ID

CODEOWNERS = ["@ssieb"]

AUTO_LOAD = ["key_provider"]

MULTI_CONF = True

wiegand_ns = cg.esphome_ns.namespace("wiegand")

Wiegand = wiegand_ns.class_("Wiegand", key_provider.KeyProvider, cg.Component)
WiegandTagTrigger = wiegand_ns.class_(
    "WiegandTagTrigger", automation.Trigger.template(cg.std_string)
)
WiegandRawTrigger = wiegand_ns.class_(
    "WiegandRawTrigger", automation.Trigger.template(cg.uint8, cg.uint64)
)
WiegandKeyTrigger = wiegand_ns.class_(
    "WiegandKeyTrigger", automation.Trigger.template(cg.uint8)
)

CONF_D0 = "d0"
CONF_D1 = "d1"
CONF_ON_RAW = "on_raw"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Wiegand),
        cv.Required(CONF_D0): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_D1): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WiegandTagTrigger),
            }
        ),
        cv.Optional(CONF_ON_RAW): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WiegandRawTrigger),
            }
        ),
        cv.Optional(CONF_ON_KEY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WiegandKeyTrigger),
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_D0])
    cg.add(var.set_d0_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_D1])
    cg.add(var.set_d1_pin(pin))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_tag_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_RAW, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_raw_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.uint8, "bits"), (cg.uint64, "value")], conf
        )

    for conf in config.get(CONF_ON_KEY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_key_trigger(trigger))
        await automation.build_automation(trigger, [(cg.uint8, "x")], conf)
