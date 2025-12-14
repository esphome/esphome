import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
    ICON_TIMELAPSE,
    UNIT_DECIBEL,
    UNIT_HERTZ,
    UNIT_SECOND,
)

from .. import CONF_LD2410S_ID, LD2410S, ld2410s_ns

LD2410SMaxDistanceNumber = ld2410s_ns.class_("LD2410SMaxDistanceNumber", number.Number)
LD2410SMinDistanceNumber = ld2410s_ns.class_("LD2410SMinDistanceNumber", number.Number)
LD2410SDelayNumber = ld2410s_ns.class_("LD2410SDelayNumber", number.Number)
LD2410SStatusReportingFreqNumber = ld2410s_ns.class_(
    "LD2410SStatusReportingFreqNumber", number.Number
)
LD2410SDistReportingFreqNumber = ld2410s_ns.class_(
    "LD2410SDistReportingFreqNumber", number.Number
)

LD2410SThresholdTriggerNumber = ld2410s_ns.class_(
    "LD2410SThresholdTriggerNumber", number.Number
)
LD2410SThresholdHoldNumber = ld2410s_ns.class_(
    "LD2410SThresholdHoldNumber", number.Number
)
LD2410SThresholdSnrNumber = ld2410s_ns.class_(
    "LD2410SThresholdSnrNumber", number.Number
)
LD2410SThresholdSelectedGateNumber = ld2410s_ns.class_(
    "LD2410SThresholdSelectedGateNumber", number.Number
)

CONF_DISTANCE_REPORTING_FREQUENCY = "distance_reporting_frequency"
CONF_MAX_DISTANCE = "max_distance"
CONF_MIN_DISTANCE = "min_distance"
CONF_NO_DELAY = "no_delay"
CONF_STATUS_REPORTING_FREQUENCY = "status_reporting_frequency"
CONF_THRESHOLD_TRIGGER = "threshold_trigger"
CONF_THRESHOLD_HOLD = "threshold_hold"
CONF_THRESHOLD_SNR = "threshold_snr"
CONF_THRESHOLD_SELECTED_GATE = "threshold_selected_gate"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Optional(CONF_MAX_DISTANCE): number.number_schema(
        LD2410SMaxDistanceNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:arrow-collapse-right",
    ),
    cv.Optional(CONF_MIN_DISTANCE): number.number_schema(
        LD2410SMinDistanceNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:arrow-collapse-left",
    ),
    cv.Optional(CONF_NO_DELAY): number.number_schema(
        LD2410SDelayNumber,
        device_class=DEVICE_CLASS_DURATION,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_SECOND,
        icon=ICON_TIMELAPSE,
    ),
    cv.Optional(CONF_STATUS_REPORTING_FREQUENCY): number.number_schema(
        LD2410SStatusReportingFreqNumber,
        device_class=DEVICE_CLASS_FREQUENCY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_PULSE,
    ),
    cv.Optional(CONF_DISTANCE_REPORTING_FREQUENCY): number.number_schema(
        LD2410SDistReportingFreqNumber,
        device_class=DEVICE_CLASS_FREQUENCY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_PULSE,
    ),
    cv.Optional(CONF_THRESHOLD_TRIGGER): number.number_schema(
        LD2410SThresholdTriggerNumber,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_DECIBEL,
        icon="mdi:pencil",
    ),
    cv.Optional(CONF_THRESHOLD_HOLD): number.number_schema(
        LD2410SThresholdHoldNumber,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_DECIBEL,
        icon="mdi:pencil",
    ),
    cv.Optional(CONF_THRESHOLD_SNR): number.number_schema(
        LD2410SThresholdSnrNumber,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_DECIBEL,
        icon="mdi:pencil",
    ),
    cv.Optional(CONF_THRESHOLD_SELECTED_GATE): number.number_schema(
        LD2410SThresholdSelectedGateNumber,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:tune-variant",
    ),
}


async def to_code(config):
    LD2410S_component = await cg.get_variable(config[CONF_LD2410S_ID])
    if max_distance_config := config.get(CONF_MAX_DISTANCE):
        n = await number.new_number(
            max_distance_config, min_value=0.7, max_value=11.2, step=0.7
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_max_distance_number(n))
    if min_distance_config := config.get(CONF_MIN_DISTANCE):
        n = await number.new_number(
            min_distance_config, min_value=0, max_value=11.2, step=0.7
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_min_distance_number(n))
    if no_delay_config := config.get(CONF_NO_DELAY):
        n = await number.new_number(no_delay_config, min_value=1, max_value=120, step=1)
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_no_delay_number(n))
    if status_reporting_freq_config := config.get(CONF_STATUS_REPORTING_FREQUENCY):
        n = await number.new_number(
            status_reporting_freq_config, min_value=0.5, max_value=8, step=0.5
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_status_reporting_freq_number(n))
    if distance_reporting_freq_config := config.get(CONF_DISTANCE_REPORTING_FREQUENCY):
        n = await number.new_number(
            distance_reporting_freq_config, min_value=0.5, max_value=8, step=0.5
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_distance_reporting_freq_number(n))

    if threshold_trigger_config := config.get(CONF_THRESHOLD_TRIGGER):
        n = await number.new_number(
            threshold_trigger_config, min_value=10, max_value=95, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_threshold_trigger_number(n))
    if threshold_hold_config := config.get(CONF_THRESHOLD_HOLD):
        n = await number.new_number(
            threshold_hold_config, min_value=10, max_value=95, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_threshold_hold_number(n))
    if threshold_snr_config := config.get(CONF_THRESHOLD_SNR):
        n = await number.new_number(
            threshold_snr_config, min_value=5, max_value=63, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_threshold_snr_number(n))
    if threshold_selected_gate_config := config.get(CONF_THRESHOLD_SELECTED_GATE):
        n = await number.new_number(
            threshold_selected_gate_config, min_value=0, max_value=15, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410S_ID])
        cg.add(LD2410S_component.set_threshold_selected_gate_number(n))
