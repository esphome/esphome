import esphome.codegen as cg
from esphome.components.ld600x import entities, request_model_sizes
from esphome.components.ld600x.const import AREA_COUNT
from esphome.types import ConfigType

from . import LD6004Component
from .const import AREA_KINDS, CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONFIG_SCHEMA = entities.sensor_schema(
    LD6004Component, CONF_LD6004_ID, area_kinds=AREA_KINDS
)


async def to_code(config: ConfigType) -> None:
    await entities.sensor_to_code(config, CONF_LD6004_ID, area_kinds=AREA_KINDS)
    if any(f"dwell_area_{i}" in config for i in range(AREA_COUNT)):
        hub = await cg.get_variable(config[CONF_LD6004_ID])
        request_model_sizes(hub, area_kinds=len(AREA_KINDS))
