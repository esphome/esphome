from esphome.components import number
from esphome.components.ld600x import entities
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6004Component
from ..const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_DWELL_LIFETIME = "dwell_lifetime"
CONF_OUTPUT_INTERVAL = "output_interval"

CONFIG_SCHEMA = entities.number_schema(
    LD6004Component,
    CONF_LD6004_ID,
    extra={
        cv.Optional(CONF_DWELL_LIFETIME): number.number_schema(
            number.ApplyNumber, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_OUTPUT_INTERVAL): number.number_schema(
            number.ApplyNumber, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    },
)
FINAL_VALIDATE_SCHEMA = entities.number_final_validate(CONF_LD6004_ID, "ld6004")


async def to_code(config: ConfigType) -> None:
    numbers = await entities.number_to_code(config, CONF_LD6004_ID)
    hub = numbers.parent
    await numbers(
        CONF_DWELL_LIFETIME,
        hub.set_dwell_lifetime_number,
        min_value=0,
        max_value=16777215,
        step=1,
        **entities.NUMBER_CONTROL,
    )
    await numbers(
        CONF_OUTPUT_INTERVAL,
        hub.set_output_interval_number,
        min_value=1,
        max_value=16777215,
        step=1,
        **entities.NUMBER_CONTROL,
    )
