import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
)
from .. import CONF_MR24HPC1_ID, mr24hpc1Component, mr24hpc1_ns

SensitivityNumber = mr24hpc1_ns.class_("SensitivityNumber", number.Number)
CustomModeNumber = mr24hpc1_ns.class_("CustomModeNumber", number.Number)
ExistenceThresholdNumber = mr24hpc1_ns.class_("ExistenceThresholdNumber", number.Number)
MotionThresholdNumber = mr24hpc1_ns.class_("MotionThresholdNumber", number.Number)
MotionTriggerTimeNumber = mr24hpc1_ns.class_("MotionTriggerTimeNumber", number.Number)
Motion2RestTimeNumber = mr24hpc1_ns.class_("Motion2RestTimeNumber", number.Number)
CustomUnmanTimeNumber = mr24hpc1_ns.class_("CustomUnmanTimeNumber", number.Number)

CONF_SENSITIVE = "sensitivity"
CONF_CUSTOMMODE = "custom_mode"
CONF_EXISTENCETHRESHOLD = "existence_threshold"
CONF_MOTIONTHRESHOLD = "motion_threshold"
CONF_MOTIONTRIGGER = "motion_trigger"
CONF_MOTION2REST = "motion_to_rest"
CONF_CUSTOMUNMANTIME = "custom_unman_time"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(mr24hpc1Component),
        cv.Optional(CONF_SENSITIVE): number.number_schema(
            SensitivityNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:archive-check-outline",
        ),
        cv.Optional(CONF_CUSTOMMODE): number.number_schema(
            CustomModeNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:cog",
        ),
        cv.Optional(CONF_EXISTENCETHRESHOLD): number.number_schema(
            ExistenceThresholdNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_MOTIONTHRESHOLD): number.number_schema(
            MotionThresholdNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_MOTIONTRIGGER): number.number_schema(
            MotionTriggerTimeNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:camera-timer",
            unit_of_measurement="ms",
        ),
        cv.Optional(CONF_MOTION2REST): number.number_schema(
            Motion2RestTimeNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:camera-timer",
            unit_of_measurement="ms",
        ),
        cv.Optional(CONF_CUSTOMUNMANTIME): number.number_schema(
            CustomUnmanTimeNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:camera-timer",
            unit_of_measurement="s",
        ),
    }
)


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if sensitivity_config := config.get(CONF_SENSITIVE):
        n = await number.new_number(
            sensitivity_config, min_value=0, max_value=3, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_sensitivity_number(n))
    if custom_mode_config := config.get(CONF_CUSTOMMODE):
        n = await number.new_number(
            custom_mode_config, min_value=0, max_value=4, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_custom_mode_number(n))
    if existence_threshold_config := config.get(CONF_EXISTENCETHRESHOLD):
        n = await number.new_number(
            existence_threshold_config, min_value=0, max_value=250, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_existence_threshold_number(n))
    if motion_threshold_config := config.get(CONF_MOTIONTHRESHOLD):
        n = await number.new_number(
            motion_threshold_config, min_value=0, max_value=250, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_motion_threshold_number(n))
    if motion_trigger_config := config.get(CONF_MOTIONTRIGGER):
        n = await number.new_number(
            motion_trigger_config, min_value=0, max_value=150, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_motion_trigger_number(n))
    if motion_to_rest_config := config.get(CONF_MOTION2REST):
        n = await number.new_number(
            motion_to_rest_config, min_value=0, max_value=3000, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_motion_to_rest_number(n))
    if custom_unman_time_config := config.get(CONF_CUSTOMUNMANTIME):
        n = await number.new_number(
            custom_unman_time_config, min_value=0, max_value=3600, step=1,
        )
        await cg.register_parented(n, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_custom_unman_time_number(n))
