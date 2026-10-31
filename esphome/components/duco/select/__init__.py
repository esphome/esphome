from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import (
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    DucoDevice,
    duco_ns,
    register_duco_device,
)

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

DucoSelect = duco_ns.class_("DucoSelect", DucoDevice, select.Select)
DucoTimeFilter = duco_ns.class_("DucoTimeFilter", DucoDevice, select.Select)

TYPE_MODE = "mode"
TYPE_TIME_FILTER = "time_filter"

TIME_FILTER_OPTIONS = ["90 days", "180 days", "270 days", "360 days"]

MODE_SCHEMA = (
    select.select_schema(DucoSelect)
    .extend(cv.polling_component_schema("3s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

TIME_FILTER_SCHEMA = (
    select.select_schema(DucoTimeFilter)
    .extend(cv.polling_component_schema("60s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_MODE: MODE_SCHEMA,
        TYPE_TIME_FILTER: TIME_FILTER_SCHEMA,
    },
    default_type=TYPE_MODE,
)


async def to_code(config: ConfigType) -> None:
    options = TIME_FILTER_OPTIONS if config[CONF_TYPE] == TYPE_TIME_FILTER else DUCO_MODE_OPTIONS
    var = await select.new_select(config, options=options)
    await register_duco_device(var, config, config[CONF_DUCO_ID])
