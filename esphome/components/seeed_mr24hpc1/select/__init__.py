import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
)
from .. import CONF_MR24HPC1_ID, MR24HPC1Component, mr24hpc1_ns

SceneModeSelect = mr24hpc1_ns.class_("SceneModeSelect", select.Select)
UnmanTimeSelect = mr24hpc1_ns.class_("UnmanTimeSelect", select.Select)
ExistenceBoundarySelect = mr24hpc1_ns.class_("ExistenceBoundarySelect", select.Select)
MotionBoundarySelect = mr24hpc1_ns.class_("MotionBoundarySelect", select.Select)

CONF_SCENE_MODE = "scene_mode"
CONF_UNMAN_TIME = "unman_time"
CONF_EXISTENCE_BOUNDARY = "existence_boundary"
CONF_MOTION_BOUNDARY = "motion_boundary"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(MR24HPC1Component),
    cv.Optional(CONF_SCENE_MODE): select.select_schema(
        SceneModeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:hoop-house",
    ),
    cv.Optional(CONF_UNMAN_TIME): select.select_schema(
        UnmanTimeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:timeline-clock",
    ),
    cv.Optional(CONF_EXISTENCE_BOUNDARY): select.select_schema(
        ExistenceBoundarySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_MOTION_BOUNDARY): select.select_schema(
        MotionBoundarySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if scenemode_config := config.get(CONF_SCENE_MODE):
        s = await select.new_select(
            scenemode_config,
            options=["None", "Living Room", "Bedroom", "Washroom", "Area Detection"],
        )
        await cg.register_parented(s, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_scene_mode_select(s))
    if unmantime_config := config.get(CONF_UNMAN_TIME):
        s = await select.new_select(
            unmantime_config,
            options=[
                "None",
                "10s",
                "30s",
                "1min",
                "2min",
                "5min",
                "10min",
                "30min",
                "60min",
            ],
        )
        await cg.register_parented(s, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_unman_time_select(s))
    if existence_boundary_config := config.get(CONF_EXISTENCE_BOUNDARY):
        s = await select.new_select(
            existence_boundary_config,
            options=[
                "0.5m",
                "1.0m",
                "1.5m",
                "2.0m",
                "2.5m",
                "3.0m",
                "3.5m",
                "4.0m",
                "4.5m",
                "5.0m",
            ],
        )
        await cg.register_parented(s, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_existence_boundary_select(s))
    if motion_boundary_config := config.get(CONF_MOTION_BOUNDARY):
        s = await select.new_select(
            motion_boundary_config,
            options=[
                "0.5m",
                "1.0m",
                "1.5m",
                "2.0m",
                "2.5m",
                "3.0m",
                "3.5m",
                "4.0m",
                "4.5m",
                "5.0m",
            ],
        )
        await cg.register_parented(s, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_motion_boundary_select(s))
