import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@zebble"]
DEPENDENCIES = ["i2c", "motion"]

CONF_LIS3DH_ID = "lis3dh_id"

#  C++ namespace / class
lis3dh_ns = cg.esphome_ns.namespace("lis3dh")
LIS3DHComponent = lis3dh_ns.class_("LIS3DHComponent", MotionComponent, i2c.I2CDevice)

CONFIG_SCHEMA = {}
