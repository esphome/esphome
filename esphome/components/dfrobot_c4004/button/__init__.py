import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_C4004_ID, C4004Component, dfrobot_c4004_ns

CONF_FACTORY_RESET = "factory_reset"
CONF_RESET = "reset"
CONF_SAVE_INSTALL_SETTINGS = "save_install_settings"
CONF_APPLY_BOUNDARY_RANGE = "apply_boundary_range"
CONF_SET_TRAJECTORY_RANGE_MODE = "set_trajectory_range_mode"
CONF_CLEAR_ALL_TAGS = "clear_all_tags"
CONF_CLEAR_PEOPLE_COUNT = "clear_people_count"

C4004FactoryResetButton = dfrobot_c4004_ns.class_(
    "C4004FactoryResetButton", button.Button
)
C4004ResetButton = dfrobot_c4004_ns.class_("C4004ResetButton", button.Button)
C4004SaveInstallSettingsButton = dfrobot_c4004_ns.class_(
    "C4004SaveInstallSettingsButton", button.Button
)
C4004ApplyBoundaryRangeButton = dfrobot_c4004_ns.class_(
    "C4004ApplyBoundaryRangeButton", button.Button
)
C4004SetTrajectoryRangeModeButton = dfrobot_c4004_ns.class_(
    "C4004SetTrajectoryRangeModeButton", button.Button
)
C4004ClearAllTagsButton = dfrobot_c4004_ns.class_(
    "C4004ClearAllTagsButton", button.Button
)
C4004ClearPeopleCountButton = dfrobot_c4004_ns.class_(
    "C4004ClearPeopleCountButton", button.Button
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_FACTORY_RESET): button.button_schema(
            C4004FactoryResetButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:restore",
        ),
        cv.Optional(CONF_RESET): button.button_schema(
            C4004ResetButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:restart",
        ),
        cv.Optional(CONF_SAVE_INSTALL_SETTINGS): button.button_schema(
            C4004SaveInstallSettingsButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:content-save-cog",
        ),
        cv.Optional(CONF_APPLY_BOUNDARY_RANGE): button.button_schema(
            C4004ApplyBoundaryRangeButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:map-marker-radius",
        ),
        cv.Optional(CONF_SET_TRAJECTORY_RANGE_MODE): button.button_schema(
            C4004SetTrajectoryRangeModeButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:map-marker-path",
        ),
        cv.Optional(CONF_CLEAR_ALL_TAGS): button.button_schema(
            C4004ClearAllTagsButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:tag-remove",
        ),
        cv.Optional(CONF_CLEAR_PEOPLE_COUNT): button.button_schema(
            C4004ClearPeopleCountButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:countertop-outline",
        ),
    }
)


async def to_code(config):
    for key in (
        CONF_FACTORY_RESET,
        CONF_RESET,
        CONF_SAVE_INSTALL_SETTINGS,
        CONF_APPLY_BOUNDARY_RANGE,
        CONF_SET_TRAJECTORY_RANGE_MODE,
        CONF_CLEAR_ALL_TAGS,
        CONF_CLEAR_PEOPLE_COUNT,
    ):
        if button_config := config.get(key):
            btn = await button.new_button(button_config)
            await cg.register_parented(btn, config[CONF_C4004_ID])
