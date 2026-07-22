import esphome.codegen as cg

CODEOWNERS = ["@ch604"]

d01_ns = cg.esphome_ns.namespace("d01")
D01Component = d01_ns.class_("D01Component", cg.PollingComponent)

