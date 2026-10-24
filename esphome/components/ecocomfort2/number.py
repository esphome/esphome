from functools import partial

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE_OFFSET,
    ENTITY_CATEGORY_CONFIG,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from . import CONF_ECOCOMFORT2_ID, ECOCOMFORT2_CLIENT_SCHEMA, ecocomfort2_ns

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONF_HUMIDITY_OFFSET = "humidity_offset"
CONF_HUMIDITY_THRESHOLD = "humidity_threshold"
CONF_LUMINOSITY_THRESHOLD = "luminosity_threshold"
CONF_VOC_THRESHOLD = "voc_threshold"

NumberSetting = ecocomfort2_ns.enum("NumberSetting", is_class=True)

# The vendor app offers four threshold levels, 0 turning the sensor off, and offsets of up to 5 either way.
# The offsets have no device class: an offset is a difference, which Home Assistant must not convert like a reading.
# key: (setting, icon, unit, min, max, step)
_NUMBERS = {
    CONF_HUMIDITY_THRESHOLD: (
        NumberSetting.NUMBER_SETTING_HUMIDITY_THRESHOLD,
        ICON_WATER_PERCENT,
        cv.UNDEFINED,
        0,
        3,
        1,
    ),
    CONF_LUMINOSITY_THRESHOLD: (
        NumberSetting.NUMBER_SETTING_LUMINOSITY_THRESHOLD,
        "mdi:brightness-5",
        cv.UNDEFINED,
        0,
        3,
        1,
    ),
    CONF_VOC_THRESHOLD: (
        NumberSetting.NUMBER_SETTING_VOC_THRESHOLD,
        "mdi:air-filter",
        cv.UNDEFINED,
        0,
        3,
        1,
    ),
    CONF_TEMPERATURE_OFFSET: (
        NumberSetting.NUMBER_SETTING_TEMPERATURE_OFFSET,
        ICON_THERMOMETER,
        UNIT_CELSIUS,
        -5,
        5,
        0.1,
    ),
    CONF_HUMIDITY_OFFSET: (
        NumberSetting.NUMBER_SETTING_HUMIDITY_OFFSET,
        ICON_WATER_PERCENT,
        UNIT_PERCENT,
        -5,
        5,
        1,
    ),
}

CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            cv.Optional(key): number.number_schema(
                number.ApplyNumber,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=icon,
                unit_of_measurement=unit,
            )
            for key, (_, icon, unit, *_) in _NUMBERS.items()
        }
    ),
    cv.has_at_least_one_key(*_NUMBERS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    numbers = number.sub_numbers(config, parent=hub)
    # The hub publishes the value the unit confirms, so the entities publish nothing themselves
    for key, (setting, _, _, min_value, max_value, step) in _NUMBERS.items():
        await numbers(
            key,
            partial(hub.set_number, setting),
            apply=f"write_number({setting}, value)",
            min_value=min_value,
            max_value=max_value,
            step=step,
        )
