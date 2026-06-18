import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@clydebarrow"]

CONF_LSM6DS_ID = "lsm6ds_id"
#  C++ namespace / class

lsm6ds_ns = cg.esphome_ns.namespace("lsm6ds")
LSM6DSComponent = lsm6ds_ns.class_(
    "LSM6DSComponent",
    MotionComponent,
    i2c.I2CDevice,
)
