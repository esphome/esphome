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

from . import CONF_LD2410S_ID, LD2410SComponent, Parameter, ld2410s_ns

DEPENDENCIES = ["ld2410s"]

CONF_DISTANCE_REPORTING_FREQUENCY = "distance_reporting_frequency"
CONF_MAX_DISTANCE = "max_distance"
CONF_MIN_DISTANCE = "min_distance"
CONF_NO_DELAY = "no_delay"
CONF_STATUS_REPORTING_FREQUENCY = "status_reporting_frequency"

LD2410SNumber = ld2410s_ns.class_("LD2410SNumber", number.Number)

# The module counts distances in 0.7 m gates and frequencies in tenths of a hertz
GATE_WIDTH_M = 0.7
_DISTANCE_RANGE = {"min_value": 0, "max_value": 16 * GATE_WIDTH_M, "step": GATE_WIDTH_M}
_FREQUENCY_RANGE = {"min_value": 0.5, "max_value": 8, "step": 0.5}

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410SComponent),
    cv.Optional(CONF_MAX_DISTANCE): number.number_schema(
        LD2410SNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_METER,
        icon="mdi:arrow-collapse-right",
    ),
    cv.Optional(CONF_MIN_DISTANCE): number.number_schema(
        LD2410SNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_METER,
        icon="mdi:arrow-collapse-left",
    ),
    cv.Optional(CONF_NO_DELAY): number.number_schema(
        LD2410SNumber,
        device_class=DEVICE_CLASS_DURATION,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_SECOND,
        icon=ICON_TIMELAPSE,
    ),
    cv.Optional(CONF_STATUS_REPORTING_FREQUENCY): number.number_schema(
        LD2410SNumber,
        device_class=DEVICE_CLASS_FREQUENCY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_PULSE,
    ),
    cv.Optional(CONF_DISTANCE_REPORTING_FREQUENCY): number.number_schema(
        LD2410SNumber,
        device_class=DEVICE_CLASS_FREQUENCY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_PULSE,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410S_ID])
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_MAX_DISTANCE,
        hub.set_max_distance_number,
        Parameter.MAX_DISTANCE,
        **_DISTANCE_RANGE,
    )
    await numbers(
        CONF_MIN_DISTANCE,
        hub.set_min_distance_number,
        Parameter.MIN_DISTANCE,
        **_DISTANCE_RANGE,
    )
    await numbers(
        CONF_NO_DELAY,
        hub.set_no_delay_number,
        Parameter.NO_DELAY,
        min_value=1,
        max_value=120,
        step=1,
    )
    await numbers(
        CONF_STATUS_REPORTING_FREQUENCY,
        hub.set_status_reporting_frequency_number,
        Parameter.STATUS_FREQUENCY,
        **_FREQUENCY_RANGE,
    )
    await numbers(
        CONF_DISTANCE_REPORTING_FREQUENCY,
        hub.set_distance_reporting_frequency_number,
        Parameter.DISTANCE_FREQUENCY,
        **_FREQUENCY_RANGE,
    )
