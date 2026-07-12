import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["i2c", "motion"]

CONF_QMI8658_ID = "qmi8658_id"
#  C++ namespace / class
qmi8658_ns = cg.esphome_ns.namespace("qmi8658")
QMI8658Component = qmi8658_ns.class_("QMI8658Component", MotionComponent, i2c.I2CDevice)

CONFIG_SCHEMA = {}
