import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor"]
DOMAIN = "ttp229_lsf"

CONF_TTP229_ID = "ttp229_id"
ttp229_lsf_ns = cg.esphome_ns.namespace("ttp229_lsf")

TTP229LSFComponent = ttp229_lsf_ns.class_(
    "TTP229LSFComponent", cg.Component, i2c.I2CDevice
)

# Publishes the channel binary sensors from the touch status read on each loop
CHANNELS_PUBLISHER = cg.HubFunction(
    "channels",
    cg.void,
    [(cg.uint16, "touched")],
    define="USE_TTP229_LSF_CHANNELS_PUBLISHER",
)
TOUCHED = CHANNELS_PUBLISHER.param("touched")

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TTP229LSFComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x57))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)
