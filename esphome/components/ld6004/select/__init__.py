import esphome.codegen as cg
from esphome.components import select
from esphome.components.ld600x import entities
from esphome.components.ld600x.const import CONF_WORK_MODE
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.core.entity_helpers import SubEntities
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from .. import LD6004Component
from ..const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_P20_MODE = "p20_mode"

CONFIG_SCHEMA = entities.select_schema(
    LD6004Component,
    CONF_LD6004_ID,
    area_kinds=("interference", "detection", "dwell"),
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
    await entities.select_to_code(
        config, CONF_LD6004_ID, area_kinds=("interference", "detection", "dwell")
    )
    hub: MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    selects: SubEntities = select.sub_selects(config, parent=hub)
    await selects(
        CONF_WORK_MODE,
        hub.set_work_mode_select,
        options=[
            "normal",
            "low_power",
            "radar_off_p20_high",
            "radar_off_p20_low",
            "high_reflectivity",
        ],
        **entities.SELECT_CONTROL,
    )
    await selects(
        CONF_P20_MODE,
        hub.set_p20_mode_select,
        options=[
            "presence_high",
            "presence_low",
            "constant_low",
            "constant_high",
            "pulse_low",
            "pulse_high",
        ],
        **entities.SELECT_CONTROL,
    )
