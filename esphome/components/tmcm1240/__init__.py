import math

from esphome import automation
import esphome.codegen as cg
from esphome.components import cia402
from esphome.components.canopen import CONF_CANOPEN_ID, CONF_NODE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["cia402"]
CODEOWNERS = ["@kj4dxk"]
DEPENDENCIES = ["canopen"]
DOMAIN = "tmcm1240"
MULTI_CONF = True

CONF_RUN_CURRENT = "run_current"
CONF_STANDBY_CURRENT = "standby_current"
CONF_STALLGUARD_THRESHOLD = "stallguard_threshold"
CONF_STALLGUARD_MIN_VELOCITY = "stallguard_min_velocity"

tmcm1240_ns = cg.esphome_ns.namespace(DOMAIN)
TMCM1240 = tmcm1240_ns.class_("TMCM1240", cia402.CiA402Drive)
COUNTS = cv.int_range(min=1, max=0x7FFFFFFF)


def validate_currents(config: ConfigType) -> ConfigType:
    if not all(
        math.isfinite(config[key]) for key in (CONF_RUN_CURRENT, CONF_STANDBY_CURRENT)
    ):
        raise cv.Invalid("Motor currents must be finite")
    if config[CONF_STANDBY_CURRENT] > config[CONF_RUN_CURRENT]:
        raise cv.Invalid("standby_current must not exceed run_current")
    return config


CONFIG_SCHEMA = cv.All(
    cia402.DRIVE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TMCM1240),
            cv.Required(CONF_RUN_CURRENT): cv.float_range(min=0.072, max=2.0),
            cv.Required(CONF_STANDBY_CURRENT): cv.float_range(min=0.072, max=2.0),
            cv.Required(CONF_STALLGUARD_THRESHOLD): cv.int_range(min=-63, max=63),
            cv.Required(CONF_STALLGUARD_MIN_VELOCITY): COUNTS,
        }
    ),
    validate_currents,
    cia402.validate_limits,
)


def validate_node_owner(config: ConfigType) -> ConfigType:
    return cia402.validate_node_owner(config, ("cia402", "tmcm1240"))


FINAL_VALIDATE_SCHEMA = validate_node_owner


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_CANOPEN_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent, config[CONF_NODE_ID])
    await cia402.register_drive(var, config)
    cg.add(var.set_run_current(config[CONF_RUN_CURRENT]))
    cg.add(var.set_standby_current(config[CONF_STANDBY_CURRENT]))
    cg.add(var.set_stallguard_threshold(config[CONF_STALLGUARD_THRESHOLD]))
    cg.add(var.set_stallguard_min_velocity(config[CONF_STALLGUARD_MIN_VELOCITY]))


ACTION_SCHEMA = cv.maybe_simple_value(
    cv.Schema({cv.GenerateID(): cv.use_id(TMCM1240)}), key=CONF_ID
)
automation.register_apply_action(
    "tmcm1240.apply_configuration",
    ACTION_SCHEMA,
    automation.ApplyCall("apply_configuration()"),
)
