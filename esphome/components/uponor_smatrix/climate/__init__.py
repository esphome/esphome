import esphome.codegen as cg
from esphome.components import climate

from .. import (
    UPONOR_SMATRIX_DEVICE_SCHEMA,
    UponorSmatrixDevice,
    register_uponor_smatrix_device,
    uponor_smatrix_ns,
)

DEPENDENCIES = ["uponor_smatrix"]

UponorSmatrixClimate = uponor_smatrix_ns.class_(
    "UponorSmatrixClimate",
    climate.Climate,
    cg.Component,
    UponorSmatrixDevice,
)

CONFIG_SCHEMA = climate.climate_schema(UponorSmatrixClimate).extend(
    UPONOR_SMATRIX_DEVICE_SCHEMA
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await register_uponor_smatrix_device(var, config)
