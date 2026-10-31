import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_TYPE
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
CONF_MODE = "mode"

TIME_FILTER_OPTIONS = ["90 days", "180 days", "270 days", "360 days"]

MODE_ENTITY_SCHEMA = (
    select.select_schema(DucoSelect)
    .extend(cv.polling_component_schema("3s"))
    .extend({cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45)})
)

LEGACY_MODE_SCHEMA = MODE_ENTITY_SCHEMA.extend(DUCO_COMPONENT_SCHEMA).extend(
    {cv.Optional(CONF_TYPE): cv.one_of(TYPE_MODE)}
)

TIME_FILTER_SCHEMA = (
    select.select_schema(DucoTimeFilter)
    .extend(cv.polling_component_schema("60s"))
    .extend(DUCO_COMPONENT_SCHEMA)
    .extend({cv.Required(CONF_TYPE): cv.one_of(TYPE_TIME_FILTER)})
)

CONFIG_SCHEMA = cv.Any(
    LEGACY_MODE_SCHEMA,
    TIME_FILTER_SCHEMA,
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Optional(CONF_MODE): cv.ensure_list(MODE_ENTITY_SCHEMA),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    if CONF_MODE in config:
        for entity_config in config[CONF_MODE]:
            var = await select.new_select(entity_config, options=DUCO_MODE_OPTIONS)
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
            cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        return

    options = TIME_FILTER_OPTIONS if config.get(CONF_TYPE) == TYPE_TIME_FILTER else DUCO_MODE_OPTIONS
    var = await select.new_select(config, options=options)
    await register_duco_device(var, config, config[CONF_DUCO_ID])
    if config.get(CONF_TYPE) != TYPE_TIME_FILTER:
        cg.add(var.set_address(config[CONF_ADDRESS]))
