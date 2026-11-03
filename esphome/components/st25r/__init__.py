from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import nfc
import esphome.config_validation as cv
from esphome.const import CONF_IRQ_PIN, CONF_ON_TAG, CONF_ON_TAG_REMOVED, CONF_RESET_PIN
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["nfc"]
CODEOWNERS = ["@JohnMcLear"]
DOMAIN = "st25r"
MULTI_CONF = True

CONF_RF_POWER = "rf_power"

st25r_ns = cg.esphome_ns.namespace("st25r")
ST25R = st25r_ns.class_("ST25R", nfc.Nfcc, cg.PollingComponent)

ST25R_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ST25R),
        cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RF_POWER, default=15): cv.int_range(min=0, max=15),
        cv.Optional(CONF_ON_TAG): automation.validate_automation({}),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation({}),
    }
).extend(cv.polling_component_schema("1s"))

_TAG_ARGS = [(cg.std_string, "x"), (nfc.NfcTagConstRef, "tag")]

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_TAG, "add_on_tag_callback", _TAG_ARGS),
    automation.CallbackAutomation(
        CONF_ON_TAG_REMOVED, "add_on_tag_removed_callback", _TAG_ARGS
    ),
)


async def setup_st25r(var: MockObj, config: ConfigType) -> None:
    await cg.add_pin(var.set_irq_pin, config.get(CONF_IRQ_PIN))
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))

    cg.add(var.set_rf_power(config[CONF_RF_POWER]))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
