import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID

from .. import (
    uponor_smatrix_ns,
    UponorSmatrixDevice,
    UPONOR_SMATRIX_DEVICE_SCHEMA,
    register_uponor_smatrix_device,
)

DEPENDENCIES = ["uponor_smatrix"]

UponorSmatrixClimate = uponor_smatrix_ns.class_(
    "UponorSmatrixClimate",
    climate.Climate,
    cg.Component,
    UponorSmatrixDevice,
)

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(UponorSmatrixClimate),
    }
).extend(UPONOR_SMATRIX_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await register_uponor_smatrix_device(var, config)
