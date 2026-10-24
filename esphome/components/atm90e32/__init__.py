import esphome.codegen as cg

CODEOWNERS = ["@circuitsetup", "@descipher"]
DOMAIN = "atm90e32"

atm90e32_ns = cg.esphome_ns.namespace("atm90e32")
ATM90E32Component = atm90e32_ns.class_("ATM90E32Component", cg.Component)

CONF_ATM90E32_ID = "atm90e32_id"
