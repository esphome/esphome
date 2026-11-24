from functools import partial

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_FREQUENCY,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
    ICON_TIMELAPSE,
    UNIT_HERTZ,
    UNIT_METER,
    UNIT_SECOND,
)
from esphome.types import ConfigType

from . import CONF_LD2410S_ID, LD2410SComponent, Parameter

DEPENDENCIES = ["ld2410s"]

CONF_DISTANCE_REPORTING_FREQUENCY = "distance_reporting_frequency"
CONF_MAX_DISTANCE = "max_distance"
CONF_MIN_DISTANCE = "min_distance"
CONF_NO_DELAY = "no_delay"
CONF_STATUS_REPORTING_FREQUENCY = "status_reporting_frequency"

# The module counts distances in 0.7 m gates and frequencies in tenths of a hertz;
# PARAMETER_SCALES in ld2410s.cpp converts with the same factors
GATE_WIDTH_M = 0.7
FREQUENCY_SCALE = 0.1
_DISTANCE_RANGE = {"min_value": 0, "max_value": 16 * GATE_WIDTH_M, "step": GATE_WIDTH_M}
_FREQUENCY_RANGE = {"min_value": 0.5, "max_value": 8, "step": 0.5}


def _number_schema(device_class: str, unit: str, icon: str) -> cv.Schema:
    return number.number_schema(
        number.ApplyNumber,
        device_class=device_class,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=unit,
        icon=icon,
    )


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410SComponent),
    cv.Optional(CONF_MAX_DISTANCE): _number_schema(
        DEVICE_CLASS_DISTANCE, UNIT_METER, "mdi:arrow-collapse-right"
    ),
    cv.Optional(CONF_MIN_DISTANCE): _number_schema(
        DEVICE_CLASS_DISTANCE, UNIT_METER, "mdi:arrow-collapse-left"
    ),
    cv.Optional(CONF_NO_DELAY): _number_schema(
        DEVICE_CLASS_DURATION, UNIT_SECOND, ICON_TIMELAPSE
    ),
    cv.Optional(CONF_STATUS_REPORTING_FREQUENCY): _number_schema(
        DEVICE_CLASS_FREQUENCY, UNIT_HERTZ, ICON_PULSE
    ),
    cv.Optional(CONF_DISTANCE_REPORTING_FREQUENCY): _number_schema(
        DEVICE_CLASS_FREQUENCY, UNIT_HERTZ, ICON_PULSE
    ),
}


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD2410S_PARAMETERS")
    hub = await cg.get_variable(config[CONF_LD2410S_ID])
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_MAX_DISTANCE,
        partial(hub.set_number, Parameter.PARAMETER_MAX_DISTANCE),
        apply="control_number(entity, value)",
        **_DISTANCE_RANGE,
    )
    await numbers(
        CONF_MIN_DISTANCE,
        partial(hub.set_number, Parameter.PARAMETER_MIN_DISTANCE),
        apply="control_number(entity, value)",
        **_DISTANCE_RANGE,
    )
    await numbers(
        CONF_NO_DELAY,
        partial(hub.set_number, Parameter.PARAMETER_NO_DELAY),
        apply="control_number(entity, value)",
        min_value=1,
        max_value=120,
        step=1,
    )
    await numbers(
        CONF_STATUS_REPORTING_FREQUENCY,
        partial(hub.set_number, Parameter.PARAMETER_STATUS_FREQUENCY),
        apply="control_number(entity, value)",
        **_FREQUENCY_RANGE,
    )
    await numbers(
        CONF_DISTANCE_REPORTING_FREQUENCY,
        partial(hub.set_number, Parameter.PARAMETER_DISTANCE_FREQUENCY),
        apply="control_number(entity, value)",
        **_FREQUENCY_RANGE,
    )
