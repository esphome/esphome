import esphome.codegen as cg
from esphome.components import number
from esphome.components.ld600x import entities
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.core.entity_helpers import SubEntities
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from .. import LD6004Component, ld6004_ns
from ..const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_DWELL_LIFETIME = "dwell_lifetime"
CONF_OUTPUT_INTERVAL = "output_interval"

LD6004NumberType = ld6004_ns.enum("LD6004NumberType")

CONFIG_SCHEMA = entities.number_schema(
    LD6004Component,
    CONF_LD6004_ID,
    extra={
        cv.Optional(CONF_DWELL_LIFETIME): number.number_schema(
            entities.LD600XNumber, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_OUTPUT_INTERVAL): number.number_schema(
            entities.LD600XNumber, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    },
)
FINAL_VALIDATE_SCHEMA = entities.number_final_validate(CONF_LD6004_ID, "ld6004")


async def to_code(config: ConfigType) -> None:
    await entities.number_to_code(config, CONF_LD6004_ID)
    hub: MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    numbers: SubEntities = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_DWELL_LIFETIME,
        hub.set_dwell_lifetime_number,
        LD6004NumberType.NUMBER_DWELL_LIFETIME,
        min_value=0,
        max_value=16777215,
        step=1,
    )
    await numbers(
        CONF_OUTPUT_INTERVAL,
        hub.set_output_interval_number,
        LD6004NumberType.NUMBER_OUTPUT_INTERVAL,
        min_value=1,
        max_value=16777215,
        step=1,
    )
