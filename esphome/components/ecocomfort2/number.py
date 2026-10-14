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

Ecocomfort2Number = ecocomfort2_ns.class_("Ecocomfort2Number", number.Number)
NumberSetting = ecocomfort2_ns.enum("NumberSetting", is_class=True)

# The vendor app offers four threshold levels, 0 turning the sensor off, and offsets of up to 5 either way
_THRESHOLD_RANGE = {"min_value": 0, "max_value": 3, "step": 1}
_THRESHOLDS = {
    CONF_HUMIDITY_THRESHOLD: (NumberSetting.HUMIDITY_THRESHOLD, ICON_WATER_PERCENT),
    CONF_LUMINOSITY_THRESHOLD: (NumberSetting.LUMINOSITY_THRESHOLD, "mdi:brightness-5"),
    CONF_VOC_THRESHOLD: (NumberSetting.VOC_THRESHOLD, "mdi:air-filter"),
}

CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            **{
                cv.Optional(key): number.number_schema(
                    Ecocomfort2Number,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=icon,
                )
                for key, (_, icon) in _THRESHOLDS.items()
            },
            # No device class: an offset is a difference, which Home Assistant must not convert like a reading
            cv.Optional(CONF_TEMPERATURE_OFFSET): number.number_schema(
                Ecocomfort2Number,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_THERMOMETER,
                unit_of_measurement=UNIT_CELSIUS,
            ),
            cv.Optional(CONF_HUMIDITY_OFFSET): number.number_schema(
                Ecocomfort2Number,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_WATER_PERCENT,
                unit_of_measurement=UNIT_PERCENT,
            ),
        }
    ),
    cv.has_at_least_one_key(
        *_THRESHOLDS, CONF_TEMPERATURE_OFFSET, CONF_HUMIDITY_OFFSET
    ),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    numbers = number.sub_numbers(config, parent=hub)
    for key, (setting, _) in _THRESHOLDS.items():
        await numbers(
            key, partial(hub.set_number, setting), setting, **_THRESHOLD_RANGE
        )
    await numbers(
        CONF_TEMPERATURE_OFFSET,
        partial(hub.set_number, NumberSetting.TEMPERATURE_OFFSET),
        NumberSetting.TEMPERATURE_OFFSET,
        min_value=-5,
        max_value=5,
        step=0.1,
    )
    await numbers(
        CONF_HUMIDITY_OFFSET,
        partial(hub.set_number, NumberSetting.HUMIDITY_OFFSET),
        NumberSetting.HUMIDITY_OFFSET,
        min_value=-5,
        max_value=5,
        step=1,
    )
