import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@clydebarrow"]

CONF_BMI270_ID = "bmi270_id"
#  C++ namespace / class
bmi270_ns = cg.esphome_ns.namespace("bmi270")
BMI270Component = bmi270_ns.class_("BMI270Component", MotionComponent, i2c.I2CDevice)
