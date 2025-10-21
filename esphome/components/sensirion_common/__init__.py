import esphome.codegen as cg
from esphome.components import i2c

CODEOWNERS = ["@martgras"]

sensirion_common_ns = cg.esphome_ns.namespace("sensirion_common")

SensirionI2CDevice = sensirion_common_ns.class_("SensirionI2CDevice", i2c.I2CDevice)
