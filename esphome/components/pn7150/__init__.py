from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, nfc
from esphome.const import CONF_ID, CONF_ON_TAG, CONF_TRIGGER_ID

AUTO_LOAD = ["nfc"]
# CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]

CONF_IRQ_PIN = "irq_pin"
CONF_VEN_PIN = "ven_pin"

pn7150_ns = cg.esphome_ns.namespace("pn7150")
PN7150 = pn7150_ns.class_("PN7150", cg.PollingComponent, i2c.I2CDevice)


# OnFinishedWriteTrigger = nfc.nfc_ns.class_(
#     "OnFinishedWriteTrigger", automation.Trigger.template()
# )

# IsWritingCondition = nfc.nfc_ns.class_(
#     "IsWritingCondition", automation.Condition
# )

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PN7150),
            cv.Optional(CONF_ON_TAG): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
                }
            ),
            # cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            #     {
            #         cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnTagTrigger),
            #     }
            # ),
            cv.Required(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_VEN_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(pin))

    pin = await cg.gpio_pin_expression(config[CONF_VEN_PIN])
    cg.add(var.set_ven_pin(pin))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x"), (nfc.NfcTag, "tag")], conf
        )
