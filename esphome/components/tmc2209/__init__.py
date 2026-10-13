import esphome.codegen as cg
from esphome.components import tmc22xx

CODEOWNERS = ["@remcom"]
DOMAIN = "tmc2209"

tmc2209_ns = cg.esphome_ns.namespace("tmc2209")
TMC2209Stepper = tmc2209_ns.class_("TMC2209Stepper", tmc22xx.TMC22XXStepper)

tmc22xx.register_actions("tmc2209", TMC2209Stepper)
