import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_RANGE,
    CONF_MIN_RANGE,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_SECOND,
)

from .. import CONF_C4001_ID, C4001Component, dfrobot_c4001_ns

MinRangeNumber = dfrobot_c4001_ns.class_("MinRangeNumber", number.Number)
MaxRangeNumber = dfrobot_c4001_ns.class_("MaxRangeNumber", number.Number)
TrigRangeNumber = dfrobot_c4001_ns.class_("TrigRangeNumber", number.Number)
KeepSensitivityNumber = dfrobot_c4001_ns.class_("KeepSensitivityNumber", number.Number)
TrigSensitivityNumber = dfrobot_c4001_ns.class_("TrigSensitivityNumber", number.Number)
ConfirmDelayNumber = dfrobot_c4001_ns.class_("ConfirmDelayNumber", number.Number)
DisappearDelayNumber = dfrobot_c4001_ns.class_("DisappearDelayNumber", number.Number)
ThresholdFactorNumber = dfrobot_c4001_ns.class_("ThresholdFactorNumber", number.Number)

CONF_TRIG_RANGE = "trig_range"
CONF_KEEP_SENSITIVITY = "keep_sensitivity"
CONF_TRIG_SENSITIVITY = "trig_sensitivity"
CONF_CONFIRM_DELAY = "confirm_delay"
CONF_DISAPPEAR_DELAY = "disappear_delay"
CONF_THRESHOLD_FACTOR = "threshold_factor"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4001_ID): cv.use_id(C4001Component),
        cv.Optional(CONF_MIN_RANGE): number.number_schema(
            MinRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:ruler",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_MAX_RANGE): number.number_schema(
            MaxRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:ruler",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_TRIG_RANGE): number.number_schema(
            TrigRangeNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:ruler",
            unit_of_measurement="m",
        ),
        cv.Optional(CONF_KEEP_SENSITIVITY): number.number_schema(
            KeepSensitivityNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:tune",
        ),
        cv.Optional(CONF_TRIG_SENSITIVITY): number.number_schema(
            TrigSensitivityNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:tune",
        ),
        cv.Optional(CONF_CONFIRM_DELAY): number.number_schema(
            ConfirmDelayNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:timer-off",
            unit_of_measurement=UNIT_SECOND,
        ),
        cv.Optional(CONF_DISAPPEAR_DELAY): number.number_schema(
            DisappearDelayNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:timer-off",
            unit_of_measurement=UNIT_SECOND,
        ),
        cv.Optional(CONF_THRESHOLD_FACTOR): number.number_schema(
            ThresholdFactorNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:chart-bell-curve",
        ),
    }
)


async def to_code(config):
    number_component = await cg.get_variable(config[CONF_C4001_ID])
    if min_config := config.get(CONF_MIN_RANGE):
        n = await number.new_number(min_config, min_value=0.3, max_value=25.0, step=0.1)
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_min_range_number(n))
    if max_config := config.get(CONF_MAX_RANGE):
        n = await number.new_number(max_config, min_value=1.0, max_value=25.0, step=0.1)
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_max_range_number(n))
    if trig_config := config.get(CONF_TRIG_RANGE):
        n = await number.new_number(
            trig_config, min_value=0.0, max_value=25.0, step=0.1
        )
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_trig_range_number(n))
    if keep_s_config := config.get(CONF_KEEP_SENSITIVITY):
        n = await number.new_number(keep_s_config, min_value=0, max_value=9, step=1)
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_keep_sensitivity_number(n))
    if trig_s_config := config.get(CONF_TRIG_SENSITIVITY):
        n = await number.new_number(trig_s_config, min_value=0, max_value=9, step=1)
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_trig_sensitivity_number(n))
    if confirm_config := config.get(CONF_CONFIRM_DELAY):
        n = await number.new_number(
            confirm_config, min_value=0, max_value=255, step=0.1
        )
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_confirm_delay_number(n))
    if disappear_config := config.get(CONF_DISAPPEAR_DELAY):
        n = await number.new_number(
            disappear_config, min_value=0, max_value=1500, step=0.1
        )
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_disappear_delay_number(n))
    if threshold_config := config.get(CONF_THRESHOLD_FACTOR):
        n = await number.new_number(threshold_config, min_value=0, max_value=20, step=1)
        await cg.register_parented(n, config[CONF_C4001_ID])
        cg.add(number_component.set_threshold_factor_number(n))
