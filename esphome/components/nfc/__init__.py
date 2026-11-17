from esphome import automation
import esphome.codegen as cg

CODEOWNERS = ["@jesserockz", "@kbx81"]
DOMAIN = "nfc"

nfc_ns = cg.esphome_ns.namespace("nfc")

Nfcc = nfc_ns.class_("Nfcc")
NfcTag = nfc_ns.class_("NfcTag")
NfcTagConstRef = NfcTag.operator("ref").operator("const")
NfcTagListener = nfc_ns.class_("NfcTagListener")
NfcOnTagTrigger = nfc_ns.class_(
    "NfcOnTagTrigger", automation.Trigger.template(cg.std_string, NfcTagConstRef)
)
