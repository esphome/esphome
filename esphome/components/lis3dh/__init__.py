from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD

CODEOWNERS = ["@zebble"]
DEPENDENCIES = ["i2c", "motion"]
DOMAIN = "lis3dh"

CONF_LIS3DH_ID = "lis3dh_id"

#  C++ namespace / class
lis3dh_ns = cg.esphome_ns.namespace("lis3dh")
LIS3DHComponent = lis3dh_ns.class_("LIS3DHComponent", MotionComponent, i2c.I2CDevice)

ACCELERATION_G = cv.All(cv.float_with_unit("acceleration", "(g|G)?"), cv.positive_float)

CONFIG_SCHEMA = {}

# Out-of-range values are clamped on the device to the steps the range allows.
automation.register_apply_action(
    "lis3dh.set_tap_threshold",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(LIS3DHComponent),
            cv.Required(CONF_THRESHOLD): cv.templatable(ACCELERATION_G),
        }
    ),
    automation.ApplyField(CONF_THRESHOLD, "set_tap_threshold", cg.float_),
)
