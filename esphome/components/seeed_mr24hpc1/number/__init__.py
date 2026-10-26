import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_SENSITIVITY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_MR24HPC1_ID, MR24HPC1Component

CONF_CUSTOM_MODE = "custom_mode"
CONF_EXISTENCE_THRESHOLD = "existence_threshold"
CONF_MOTION_THRESHOLD = "motion_threshold"
CONF_MOTION_TRIGGER = "motion_trigger"
CONF_MOTION_TO_REST = "motion_to_rest"
CONF_CUSTOM_UNMAN_TIME = "custom_unman_time"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(MR24HPC1Component),
        cv.Optional(CONF_SENSITIVITY): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:archive-check-outline",
        ),
        cv.Optional(CONF_CUSTOM_MODE): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:cog",
        ),
        cv.Optional(CONF_EXISTENCE_THRESHOLD): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_MOTION_THRESHOLD): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_MOTION_TRIGGER): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:camera-timer",
            unit_of_measurement="ms",
        ),
        cv.Optional(CONF_MOTION_TO_REST): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:camera-timer",
            unit_of_measurement="ms",
        ),
        cv.Optional(CONF_CUSTOM_UNMAN_TIME): number.number_schema(
            number.ApplyNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:camera-timer",
            unit_of_measurement="s",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR24HPC1_ID])
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_SENSITIVITY,
        hub.set_sensitivity_number,
        min_value=0,
        max_value=3,
        step=1,
        apply="set_sensitivity(value)",
    )
    await numbers(
        CONF_CUSTOM_MODE,
        hub.set_custom_mode_number,
        min_value=0,
        max_value=4,
        step=1,
        apply="set_custom_mode(value)",
        publish="before",
    )
    await numbers(
        CONF_EXISTENCE_THRESHOLD,
        hub.set_existence_threshold_number,
        min_value=0,
        max_value=250,
        step=1,
        apply="set_existence_threshold(value)",
    )
    await numbers(
        CONF_MOTION_THRESHOLD,
        hub.set_motion_threshold_number,
        min_value=0,
        max_value=250,
        step=1,
        apply="set_motion_threshold(value)",
    )
    await numbers(
        CONF_MOTION_TRIGGER,
        hub.set_motion_trigger_number,
        min_value=0,
        max_value=150,
        step=1,
        apply="set_motion_trigger_time(value)",
    )
    await numbers(
        CONF_MOTION_TO_REST,
        hub.set_motion_to_rest_number,
        min_value=0,
        max_value=3000,
        step=1,
        apply="set_motion_to_rest_time(value)",
    )
    await numbers(
        CONF_CUSTOM_UNMAN_TIME,
        hub.set_custom_unman_time_number,
        min_value=0,
        max_value=3600,
        step=1,
        apply="set_custom_unman_time(value)",
    )
