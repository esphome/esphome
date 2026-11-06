import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "bmi270"

CONF_BMI270_ID = "bmi270_id"
#  C++ namespace / class
bmi270_ns = cg.esphome_ns.namespace("bmi270")
BMI270Component = bmi270_ns.class_("BMI270Component", MotionComponent, i2c.I2CDevice)
BMM150Data = bmi270_ns.namespace("bmm150").class_("BMM150Data")

# Magnetic field sensors need the BMM150, so USE_BMI270_BMM150 already gates the member
MAGNETOMETER_PUBLISHER = cg.HubFunction(
    "magnetometer",
    cg.void,
    [(BMM150Data.const_ref(), "data")],
)
MAGNETOMETER_DATA = MAGNETOMETER_PUBLISHER.param("data")
