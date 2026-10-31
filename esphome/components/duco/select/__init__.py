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

# Same order as MODE_CODES in mode.cpp
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
DucoBypassControl = duco_ns.class_("DucoBypassControl", DucoDevice, select.Select)
DucoBypassAdaptiveControl = duco_ns.class_("DucoBypassAdaptiveControl", DucoDevice, select.Select)
DucoFrostHeater = duco_ns.class_("DucoFrostHeater", DucoDevice, select.Select)
DucoPassiveCooling = duco_ns.class_("DucoPassiveCooling", DucoDevice, select.Select)
DucoNightboost = duco_ns.class_("DucoNightboost", DucoDevice, select.Select)
DucoNightboostMonthStart = duco_ns.class_("DucoNightboostMonthStart", DucoDevice, select.Select)
DucoNightboostMonthStop = duco_ns.class_("DucoNightboostMonthStop", DucoDevice, select.Select)

TYPE_MODE = "mode"
TYPE_TIME_FILTER = "time_filter"
CONF_MODE = "mode"
CONF_BYPASS = "bypass"
CONF_ADAPTIVE_BYPASS = "adaptive_bypass"
CONF_FROST_HEATER = "frost_heater"
CONF_PASSIVE_COOLING = "passive_cooling"
CONF_NIGHTBOOST = "nightboost"
CONF_NIGHTBOOST_MONTH_START = "nightboost_month_start"
CONF_NIGHTBOOST_MONTH_STOP = "nightboost_month_stop"

TIME_FILTER_OPTIONS = ["90 days", "180 days", "270 days", "360 days"]
BYPASS_OPTIONS = ["AUTO", "OPEN", "CLOSED"]
ADAPTIVE_BYPASS_OPTIONS = ["ON", "OFF"]
FROST_HEATER_OPTIONS = ["ALLOW", "REFUSE"]
PASSIVE_COOLING_OPTIONS = ["AUTO", "ON", "OFF"]
NIGHTBOOST_OPTIONS = ["ON", "OFF"]
MONTH_OPTIONS = [
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
]

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

BYPASS_SCHEMA = select.select_schema(DucoBypassControl).extend(cv.polling_component_schema("60s"))

ADAPTIVE_BYPASS_SCHEMA = select.select_schema(DucoBypassAdaptiveControl).extend(
    cv.polling_component_schema("60s")
)

FROST_HEATER_SCHEMA = select.select_schema(DucoFrostHeater).extend(cv.polling_component_schema("60s"))

PASSIVE_COOLING_SCHEMA = select.select_schema(DucoPassiveCooling).extend(cv.polling_component_schema("60s"))

NIGHTBOOST_SCHEMA = select.select_schema(DucoNightboost).extend(cv.polling_component_schema("60s"))

NIGHTBOOST_MONTH_START_SCHEMA = select.select_schema(DucoNightboostMonthStart).extend(cv.polling_component_schema("60s"))

NIGHTBOOST_MONTH_STOP_SCHEMA = select.select_schema(DucoNightboostMonthStop).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = cv.Any(
    LEGACY_MODE_SCHEMA,
    TIME_FILTER_SCHEMA,
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Optional(CONF_MODE): cv.ensure_list(MODE_ENTITY_SCHEMA),
            cv.Optional(CONF_BYPASS): BYPASS_SCHEMA,
            cv.Optional(CONF_ADAPTIVE_BYPASS): ADAPTIVE_BYPASS_SCHEMA,
            cv.Optional(CONF_FROST_HEATER): FROST_HEATER_SCHEMA,
            cv.Optional(CONF_PASSIVE_COOLING): PASSIVE_COOLING_SCHEMA,
            cv.Optional(CONF_NIGHTBOOST): NIGHTBOOST_SCHEMA,
            cv.Optional(CONF_NIGHTBOOST_MONTH_START): NIGHTBOOST_MONTH_START_SCHEMA,
            cv.Optional(CONF_NIGHTBOOST_MONTH_STOP): NIGHTBOOST_MONTH_STOP_SCHEMA,
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    grouped = False
    if CONF_MODE in config:
        grouped = True
        for entity_config in config[CONF_MODE]:
            var = await select.new_select(entity_config, options=DUCO_MODE_OPTIONS)
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
            cg.add(var.set_address(entity_config[CONF_ADDRESS]))

    if entity_config := config.get(CONF_BYPASS):
        grouped = True
        var = await select.new_select(entity_config, options=BYPASS_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_ADAPTIVE_BYPASS):
        grouped = True
        var = await select.new_select(entity_config, options=ADAPTIVE_BYPASS_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_FROST_HEATER):
        grouped = True
        var = await select.new_select(entity_config, options=FROST_HEATER_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_PASSIVE_COOLING):
        grouped = True
        var = await select.new_select(entity_config, options=PASSIVE_COOLING_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_NIGHTBOOST):
        grouped = True
        var = await select.new_select(entity_config, options=NIGHTBOOST_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_NIGHTBOOST_MONTH_START):
        grouped = True
        var = await select.new_select(entity_config, options=MONTH_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_NIGHTBOOST_MONTH_STOP):
        grouped = True
        var = await select.new_select(entity_config, options=MONTH_OPTIONS)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if grouped:
        return

    options = TIME_FILTER_OPTIONS if config.get(CONF_TYPE) == TYPE_TIME_FILTER else DUCO_MODE_OPTIONS
    var = await select.new_select(config, options=options)
    await register_duco_device(var, config, config[CONF_DUCO_ID])
    if config.get(CONF_TYPE) != TYPE_TIME_FILTER:
        cg.add(var.set_address(config[CONF_ADDRESS]))
