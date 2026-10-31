from esphome.components import select
from esphome.components.ld600x import entities, request_model_sizes
from esphome.components.ld600x.const import CONF_WORK_MODE
import esphome.config_validation as cv
from esphome.const import CONF_AREA_ID, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6004Component
from ..const import AREA_KINDS, CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_P20_MODE = "p20_mode"

# The order is the wire contract: index i sends entry i of WORK_MODE_COMMANDS / P20_MODE_COMMANDS in ld6004.cpp.
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
        cv.Optional(CONF_WORK_MODE): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_P20_MODE): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
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
    if CONF_AREA_ID in config:
        # The area_id select can pick a dwell area.
        request_model_sizes(hub, area_kinds=len(AREA_KINDS))
    await selects(
        CONF_WORK_MODE,
        hub.set_work_mode_select,
        options=list(WORK_MODE_OPTIONS),
        **entities.SELECT_CONTROL,
    )
    await selects(
        CONF_P20_MODE,
        hub.set_p20_mode_select,
        options=list(P20_MODE_OPTIONS),
        **entities.SELECT_CONTROL,
    )
