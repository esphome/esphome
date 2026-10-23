import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

# Same order as MODE_CODES in select.cpp
DUCO_MODE_OPTIONS = [
    "AUTO",
    "MAN1",
    "MAN2",
    "MAN3",
    "EMPT",
    "CNT1",
    "CNT2",
    "CNT3",
    "MAN1x2",
    "MAN2x2",
    "MAN3x2",
    "MAN1x3",
    "MAN2x3",
    "MAN3x3",
]

DucoSelect = duco_ns.class_("DucoSelect", cg.PollingComponent, select.Select)

CONFIG_SCHEMA = (
    select.select_schema(DucoSelect)
    .extend(cv.polling_component_schema("3s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await select.new_select(config, options=DUCO_MODE_OPTIONS)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DUCO_ID])
