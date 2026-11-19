from esphome import automation
import esphome.codegen as cg
from esphome.components import tmc22xx
import esphome.config_validation as cv
from esphome.const import CONF_THRESHOLD

CODEOWNERS = ["@remcom"]
DOMAIN = "tmc2209"

CONF_SEDN = "sedn"
CONF_SEIMIN = "seimin"
CONF_SEMAX = "semax"
CONF_SEMIN = "semin"
CONF_SEUP = "seup"
CONF_TCOOL_THRESHOLD = "tcool_threshold"

tmc2209_ns = cg.esphome_ns.namespace("tmc2209")
TMC2209Stepper = tmc2209_ns.class_("TMC2209Stepper", tmc22xx.TMC22XXStepper)

tmc22xx.register_actions("tmc2209", TMC2209Stepper)

automation.register_apply_action(
    "tmc2209.stallguard",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TMC2209Stepper),
            cv.Optional(CONF_THRESHOLD): cv.templatable(cv.uint8_t),
            cv.Optional(CONF_TCOOL_THRESHOLD): cv.templatable(
                cv.int_range(0, 2**20 - 1)
            ),
        }
    ),
    automation.ApplyField(CONF_THRESHOLD, "set_stallguard_threshold", cg.uint8),
    automation.ApplyField(CONF_TCOOL_THRESHOLD, "set_tcool_threshold", cg.uint32),
)

automation.register_apply_action(
    "tmc2209.coolconf",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TMC2209Stepper),
            cv.Optional(CONF_SEMIN): cv.templatable(cv.int_range(0, 15)),
            cv.Optional(CONF_SEMAX): cv.templatable(cv.int_range(0, 15)),
            cv.Optional(CONF_SEUP): cv.templatable(cv.int_range(0, 3)),
            cv.Optional(CONF_SEDN): cv.templatable(cv.int_range(0, 3)),
            cv.Optional(CONF_SEIMIN): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyField(CONF_SEMIN, "set_semin", cg.uint8),
    automation.ApplyField(CONF_SEMAX, "set_semax", cg.uint8),
    automation.ApplyField(CONF_SEUP, "set_seup", cg.uint8),
    automation.ApplyField(CONF_SEDN, "set_sedn", cg.uint8),
    automation.ApplyField(CONF_SEIMIN, "set_seimin", cg.bool_),
)
