import esphome.codegen as cg
from esphome.components import spi
from esphome.components.motion import MotionComponent

CODEOWNERS = ["@lboue"]
DOMAIN = "icm20689"

CONF_ICM20689_ID = "icm20689_id"
#  C++ namespace / class
icm20689_ns = cg.esphome_ns.namespace("icm20689")
ICM20689Component = icm20689_ns.class_(
    "ICM20689Component", MotionComponent, spi.SPIDevice
)
