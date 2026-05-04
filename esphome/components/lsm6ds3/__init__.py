import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["i2c", "motion"]

CONF_LSM6DS3_ID = "lsm6ds3_id"
#  C++ namespace / class

lsm6ds3trc_ns = cg.esphome_ns.namespace("lsm6ds3")
LSM6DS3TRCComponent = lsm6ds3trc_ns.class_(
    "LSM6DS3TRCComponent",
    MotionComponent,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = {}
