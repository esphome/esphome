from esphome import automation
import esphome.codegen as cg

CODEOWNERS = ["@jesserockz"]

nfc_ns = cg.esphome_ns.namespace("nfc")

NfcTag = nfc_ns.class_("NfcTag")

NfcOnTagTrigger = nfc_ns.class_(
    "NfcOnTagTrigger", automation.Trigger.template(cg.std_string, NfcTag)
)
