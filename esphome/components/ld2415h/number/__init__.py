import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    CONF_SENSITIVITY,
    UNIT_DEGREES,
    UNIT_EMPTY,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_SECOND,
)
from .. import CONF_LD2415H_ID, LD2415HComponent, ld2415h_ns

ICON_COMPENSATION_ANGLE = "mdi:angle-acute"
ICON_SENSITIVITY = "mdi:ear-hearing"
ICON_SPEEDOMETER = "mdi:speedometer"
ICON_TIMELAPSE = "mdi:timelapse"
ICON_VIBRATE = "mdi:vibrate"

CONF_MIN_SPEED_THRESHOLD = "min_speed_threshold"
CONF_COMPENSATION_ANGLE = "compensation_angle"
CONF_VIBRATION_CORRECTION = "vibration_correction"
CONF_RELAY_TRIGGER_DURATION = "relay_trigger_duration"
CONF_RELAY_TRIGGER_SPEED = "relay_trigger_speed"

MinSpeedThresholdNumber = ld2415h_ns.class_("MinSpeedThresholdNumber", number.Number)
CompensationAngleNumber = ld2415h_ns.class_("CompensationAngleNumber", number.Number)
SensitivityNumber = ld2415h_ns.class_("SensitivityNumber", number.Number)
VibrationCorrectionNumber = ld2415h_ns.class_(
    "VibrationCorrectionNumber", number.Number
)
RelayTriggerDurationNumber = ld2415h_ns.class_(
    "RelayTriggerDurationNumber", number.Number
)
RelayTriggerSpeedNumber = ld2415h_ns.class_("RelayTriggerSpeedNumber", number.Number)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2415H_ID): cv.use_id(LD2415HComponent),
    cv.Optional(CONF_MIN_SPEED_THRESHOLD): number.number_schema(
        MinSpeedThresholdNumber,
        unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SPEEDOMETER,
    ),
    cv.Optional(CONF_COMPENSATION_ANGLE): number.number_schema(
        CompensationAngleNumber,
        unit_of_measurement=UNIT_DEGREES,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_COMPENSATION_ANGLE,
    ),
    cv.Optional(CONF_SENSITIVITY): number.number_schema(
        SensitivityNumber,
        unit_of_measurement=UNIT_EMPTY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SENSITIVITY,
    ),
    cv.Optional(CONF_VIBRATION_CORRECTION): number.number_schema(
        VibrationCorrectionNumber,
        unit_of_measurement=UNIT_EMPTY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_VIBRATE,
    ),
    cv.Optional(CONF_RELAY_TRIGGER_DURATION): number.number_schema(
        RelayTriggerDurationNumber,
        unit_of_measurement=UNIT_SECOND,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_TIMELAPSE,
    ),
    cv.Optional(CONF_RELAY_TRIGGER_SPEED): number.number_schema(
        RelayTriggerSpeedNumber,
        unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SPEEDOMETER,
    ),
}


async def to_code(config):
    ld2415h_component = await cg.get_variable(config[CONF_LD2415H_ID])
    if min_speed_threshold_config := config.get(CONF_MIN_SPEED_THRESHOLD):
        num = await number.new_number(
            min_speed_threshold_config,
            min_value=1,
            max_value=60,
            step=1,
        )
        await cg.register_parented(num, config[CONF_LD2415H_ID])
        cg.add(ld2415h_component.set_min_speed_threshold_number(num))
    if compensation_angle_config := config.get(CONF_COMPENSATION_ANGLE):
        num = await number.new_number(
            compensation_angle_config,
            min_value=0,
            max_value=90,
            step=1,
        )
        await cg.register_parented(num, config[CONF_LD2415H_ID])
        cg.add(ld2415h_component.set_compensation_angle_number(num))
    if sensitivity_config := config.get(CONF_SENSITIVITY):
        num = await number.new_number(
            sensitivity_config,
            min_value=0,
            max_value=15,
            step=1,
        )
        await cg.register_parented(num, config[CONF_LD2415H_ID])
        cg.add(ld2415h_component.set_sensitivity_number(num))
    if vibration_correction_config := config.get(CONF_VIBRATION_CORRECTION):
        num = await number.new_number(
            vibration_correction_config,
            min_value=0,
            max_value=112,
            step=1,
        )
        await cg.register_parented(num, config[CONF_LD2415H_ID])
        cg.add(ld2415h_component.set_vibration_correction_number(num))
    if relay_trigger_duration_config := config.get(CONF_RELAY_TRIGGER_DURATION):
        num = await number.new_number(
            relay_trigger_duration_config,
            min_value=0,
            max_value=255,
            step=1,
        )
        await cg.register_parented(num, config[CONF_LD2415H_ID])
        cg.add(ld2415h_component.set_relay_trigger_duration_number(num))
    if relay_trigger_speed_config := config.get(CONF_RELAY_TRIGGER_SPEED):
        num = await number.new_number(
            relay_trigger_speed_config,
            min_value=0,
            max_value=255,
            step=1,
        )
        await cg.register_parented(num, config[CONF_LD2415H_ID])
        cg.add(ld2415h_component.set_relay_trigger_speed_number(num))
