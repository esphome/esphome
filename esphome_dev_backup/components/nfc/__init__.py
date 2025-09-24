from esphome import automation
import esphome.codegen as cg

CODEOWNERS = ["@jesserockz", "@kbx81"]

nfc_ns = cg.esphome_ns.namespace("nfc")

Nfcc = nfc_ns.class_("Nfcc")
NfcTag = nfc_ns.class_("NfcTag")
NfcTagListener = nfc_ns.class_("NfcTagListener")
NfcOnTagTrigger = nfc_ns.class_(
    "NfcOnTagTrigger", automation.Trigger.template(cg.std_string, NfcTag)
)
