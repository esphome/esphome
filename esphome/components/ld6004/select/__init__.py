from esphome.components import select
from esphome.components.ld600x import entities
from esphome.components.ld600x.const import CONF_WORK_MODE
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6004Component
from ..const import AREA_KINDS, CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_P20_MODE = "p20_mode"

# The order is the wire contract: index i sends entry i of WORK_MODE_COMMANDS / P20_MODE_COMMANDS in ld6004.cpp.
# tests/component_tests/ld6004/test_select_options.py pins this order. The C++ test in tests/components/ld6004/select
# keeps its own copy of the names to pin the command each index sends, so it cannot see a reorder here.
# The work_mode names are written again in WorkModeStrings in ld6004.cpp, for the work mode text sensor.
WORK_MODE_OPTIONS = (
    "normal",
    "low_power",
    "radar_off_p20_high",
    "radar_off_p20_low",
    "high_reflectivity",
)
P20_MODE_OPTIONS = (
    "presence_high",
    "presence_low",
    "constant_low",
    "constant_high",
    "pulse_low",
    "pulse_high",
)

CONFIG_SCHEMA = entities.select_schema(
    LD6004Component,
    CONF_LD6004_ID,
    extra={
        cv.Optional(key): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        )
        for key in (CONF_WORK_MODE, CONF_P20_MODE)
    },
)
FINAL_VALIDATE_SCHEMA = entities.unique_settings_final_validate(
    select.DOMAIN, CONF_LD6004_ID, "ld6004"
)


async def to_code(config: ConfigType) -> None:
    selects = await entities.select_to_code(
        config, CONF_LD6004_ID, area_kinds=AREA_KINDS
    )
    hub = selects.parent
    for key, setter, options in (
        (CONF_WORK_MODE, hub.set_work_mode_select, WORK_MODE_OPTIONS),
        (CONF_P20_MODE, hub.set_p20_mode_select, P20_MODE_OPTIONS),
    ):
        await selects(key, setter, options=list(options), **entities.SELECT_CONTROL)
