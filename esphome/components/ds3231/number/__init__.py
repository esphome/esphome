import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    ICON_TIMER,
    UNIT_SECOND,
)
from esphome.types import ConfigType

from .. import (
    CONF_DS3231_ID,
    CONF_REFRESH_INTERVAL,
    USE_DS3231_AGING_OFFSET,
    USE_DS3231_REFRESH_INTERVAL,
    DS3231Component,
    ds3231_ns,
)

DEPENDENCIES = ["ds3231"]

CONF_AGING_OFFSET = "aging_offset"

# The aging offset trim register is a signed 8-bit value; each step is ~0.1 ppm.
AGING_OFFSET_MIN = -128
AGING_OFFSET_MAX = 127
AGING_OFFSET_STEP = 1

# Poll-interval bounds in seconds.
REFRESH_INTERVAL_MIN = 1
REFRESH_INTERVAL_MAX = 3600
REFRESH_INTERVAL_STEP = 1

DS3231AgingOffsetNumber = ds3231_ns.class_(
    "DS3231AgingOffsetNumber",
    number.Number,
    cg.Component,
    cg.Parented.template(DS3231Component),
)
DS3231RefreshIntervalNumber = ds3231_ns.class_(
    "DS3231RefreshIntervalNumber",
    number.Number,
    cg.Component,
    cg.Parented.template(DS3231Component),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        cv.Optional(CONF_AGING_OFFSET): number.number_schema(
            DS3231AgingOffsetNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:tune",
        ).extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=AGING_OFFSET_MIN): cv.int_range(
                    min=AGING_OFFSET_MIN, max=AGING_OFFSET_MAX
                ),
                cv.Optional(CONF_MAX_VALUE, default=AGING_OFFSET_MAX): cv.int_range(
                    min=AGING_OFFSET_MIN, max=AGING_OFFSET_MAX
                ),
                cv.Optional(CONF_STEP, default=AGING_OFFSET_STEP): cv.positive_int,
            }
        ),
        cv.Optional(CONF_REFRESH_INTERVAL): number.number_schema(
            DS3231RefreshIntervalNumber,
            unit_of_measurement=UNIT_SECOND,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_TIMER,
        ).extend(
            {
                cv.Optional(
                    CONF_MIN_VALUE, default=REFRESH_INTERVAL_MIN
                ): cv.positive_not_null_int,
                cv.Optional(
                    CONF_MAX_VALUE, default=REFRESH_INTERVAL_MAX
                ): cv.positive_not_null_int,
                cv.Optional(
                    CONF_STEP, default=REFRESH_INTERVAL_STEP
                ): cv.positive_not_null_int,
            }
        ),
    }
).add_extra(cv.has_at_least_one_key(CONF_AGING_OFFSET, CONF_REFRESH_INTERVAL))


async def to_code(config: ConfigType) -> None:
    if (conf := config.get(CONF_AGING_OFFSET)) is not None:
        cg.add_define(USE_DS3231_AGING_OFFSET)
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_component(var, conf)
        await cg.register_parented(var, config[CONF_DS3231_ID])

    if (conf := config.get(CONF_REFRESH_INTERVAL)) is not None:
        cg.add_define(USE_DS3231_REFRESH_INTERVAL)
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_component(var, conf)
        await cg.register_parented(var, config[CONF_DS3231_ID])
