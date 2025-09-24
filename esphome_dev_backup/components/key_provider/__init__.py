import esphome.codegen as cg

CODEOWNERS = ["@ssieb"]

key_provider_ns = cg.esphome_ns.namespace("key_provider")
KeyProvider = key_provider_ns.class_("KeyProvider")
