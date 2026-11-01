from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD
from esphome.core import CORE, ID

CODEOWNERS = ["@zebble"]
DEPENDENCIES = ["i2c", "motion"]
DOMAIN = "lis3dh"

CONF_LIS3DH_ID = "lis3dh_id"
KEY_TAP_THRESHOLD_IDS = "lis3dh_tap_threshold_ids"

#  C++ namespace / class
lis3dh_ns = cg.esphome_ns.namespace("lis3dh")
LIS3DHComponent = lis3dh_ns.class_("LIS3DHComponent", MotionComponent, i2c.I2CDevice)

ACCELERATION_G = cv.All(cv.float_with_unit("acceleration", "(g|G)?"), cv.positive_float)

CONFIG_SCHEMA = {}


def _tap_threshold_id(value: object) -> ID:
    # Checked against the tap block in the motion platform's final validation
    value = cv.use_id(LIS3DHComponent)(value)
    CORE.data.setdefault(KEY_TAP_THRESHOLD_IDS, set()).add(value.id)
    return value


# Clamped on the device to the steps the range allows
automation.register_apply_action(
    "lis3dh.set_tap_threshold",
    cv.Schema(
        {
            cv.Required(CONF_ID): _tap_threshold_id,
            cv.Required(CONF_THRESHOLD): cv.templatable(ACCELERATION_G),
        }
    ),
    automation.ApplyField(CONF_THRESHOLD, "set_tap_threshold", cg.float_),
)
