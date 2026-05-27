import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_C4004_ID, C4004Component, dfrobot_c4004_ns

CONF_PRESENCE_ENABLE = "presence_enable"
CONF_TRAJECTORY_TRACKING = "trajectory_tracking"
CONF_TRAJECTORY_LED = "trajectory_led"
CONF_MOTION_LED = "motion_led"

C4004PresenceEnableSwitch = dfrobot_c4004_ns.class_(
    "C4004PresenceEnableSwitch", switch.Switch
)
C4004TrajectoryTrackingSwitch = dfrobot_c4004_ns.class_(
    "C4004TrajectoryTrackingSwitch", switch.Switch
)
C4004TrajectoryLedSwitch = dfrobot_c4004_ns.class_(
    "C4004TrajectoryLedSwitch", switch.Switch
)
C4004MotionLedSwitch = dfrobot_c4004_ns.class_(
    "C4004MotionLedSwitch", switch.Switch
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_PRESENCE_ENABLE): switch.switch_schema(
            C4004PresenceEnableSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:human-handsup",
        ),
        cv.Optional(CONF_TRAJECTORY_TRACKING): switch.switch_schema(
            C4004TrajectoryTrackingSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:map-marker-path",
        ),
        cv.Optional(CONF_TRAJECTORY_LED): switch.switch_schema(
            C4004TrajectoryLedSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:led-on",
        ),
        cv.Optional(CONF_MOTION_LED): switch.switch_schema(
            C4004MotionLedSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:led-on",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4004_ID])

    if presence_enable_config := config.get(CONF_PRESENCE_ENABLE):
        sw = await switch.new_switch(presence_enable_config)
        await cg.register_parented(sw, config[CONF_C4004_ID])
        cg.add(parent.set_presence_enable_switch(sw))

    if trajectory_tracking_config := config.get(CONF_TRAJECTORY_TRACKING):
        sw = await switch.new_switch(trajectory_tracking_config)
        await cg.register_parented(sw, config[CONF_C4004_ID])
        cg.add(parent.set_trajectory_tracking_switch(sw))

    if trajectory_led_config := config.get(CONF_TRAJECTORY_LED):
        sw = await switch.new_switch(trajectory_led_config)
        await cg.register_parented(sw, config[CONF_C4004_ID])
        cg.add(parent.set_trajectory_led_switch(sw))

    if motion_led_config := config.get(CONF_MOTION_LED):
        sw = await switch.new_switch(motion_led_config)
        await cg.register_parented(sw, config[CONF_C4004_ID])
        cg.add(parent.set_motion_led_switch(sw))
