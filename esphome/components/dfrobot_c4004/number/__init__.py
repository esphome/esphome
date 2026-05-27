import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_C4004_ID, C4004Component, dfrobot_c4004_ns

CONF_INSTALL_HEIGHT = "install_height"
CONF_INSTALL_Z_ANGLE = "install_z_angle"
CONF_RANGE_X_MAX = "range_x_max"
CONF_RANGE_X_MIN = "range_x_min"
CONF_RANGE_Y_MAX = "range_y_max"
CONF_RANGE_Y_MIN = "range_y_min"
CONF_TARGET_COUNT = "target_count"
CONF_PEOPLE_REPORT_INTERVAL = "people_report_interval"
CONF_TRAJECTORY_GENERATE_DISTANCE = "trajectory_generate_distance"
CONF_TRAJECTORY_HOLD_TIME = "trajectory_hold_time"
CONF_NO_PERSON_DELAY = "no_person_delay"

C4004InstallHeightNumber = dfrobot_c4004_ns.class_(
    "C4004InstallHeightNumber", number.Number
)
C4004InstallZAngleNumber = dfrobot_c4004_ns.class_(
    "C4004InstallZAngleNumber", number.Number
)
C4004RangeXMaxNumber = dfrobot_c4004_ns.class_("C4004RangeXMaxNumber", number.Number)
C4004RangeXMinNumber = dfrobot_c4004_ns.class_("C4004RangeXMinNumber", number.Number)
C4004RangeYMaxNumber = dfrobot_c4004_ns.class_("C4004RangeYMaxNumber", number.Number)
C4004RangeYMinNumber = dfrobot_c4004_ns.class_("C4004RangeYMinNumber", number.Number)
C4004TargetCountNumber = dfrobot_c4004_ns.class_(
    "C4004TargetCountNumber", number.Number
)
C4004PeopleReportIntervalNumber = dfrobot_c4004_ns.class_(
    "C4004PeopleReportIntervalNumber", number.Number
)
C4004TrajectoryGenerateDistanceNumber = dfrobot_c4004_ns.class_(
    "C4004TrajectoryGenerateDistanceNumber", number.Number
)
C4004TrajectoryHoldTimeNumber = dfrobot_c4004_ns.class_(
    "C4004TrajectoryHoldTimeNumber", number.Number
)
C4004NoPersonDelayNumber = dfrobot_c4004_ns.class_(
    "C4004NoPersonDelayNumber", number.Number
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_INSTALL_HEIGHT): number.number_schema(
            C4004InstallHeightNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:human-male-height",
            unit_of_measurement="cm",
        ),
        cv.Optional(CONF_INSTALL_Z_ANGLE): number.number_schema(
            C4004InstallZAngleNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:angle-acute",
            unit_of_measurement="deg",
        ),
        cv.Optional(CONF_RANGE_X_MAX): number.number_schema(
            C4004RangeXMaxNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:axis-x-arrow",
            unit_of_measurement="cm",
        ),
        cv.Optional(CONF_RANGE_X_MIN): number.number_schema(
            C4004RangeXMinNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:axis-x-arrow",
            unit_of_measurement="cm",
        ),
        cv.Optional(CONF_RANGE_Y_MAX): number.number_schema(
            C4004RangeYMaxNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:axis-y-arrow",
            unit_of_measurement="cm",
        ),
        cv.Optional(CONF_RANGE_Y_MIN): number.number_schema(
            C4004RangeYMinNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:axis-y-arrow",
            unit_of_measurement="cm",
        ),
        cv.Optional(CONF_TARGET_COUNT): number.number_schema(
            C4004TargetCountNumber,
            icon="mdi:target-account",
            unit_of_measurement="targets",
        ),
        cv.Optional(CONF_PEOPLE_REPORT_INTERVAL): number.number_schema(
            C4004PeopleReportIntervalNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:timer-sync",
            unit_of_measurement="s",
        ),
        cv.Optional(CONF_TRAJECTORY_GENERATE_DISTANCE): number.number_schema(
            C4004TrajectoryGenerateDistanceNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:map-marker-distance",
            unit_of_measurement="cm",
        ),
        cv.Optional(CONF_TRAJECTORY_HOLD_TIME): number.number_schema(
            C4004TrajectoryHoldTimeNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:timer-outline",
            unit_of_measurement="s",
        ),
        cv.Optional(CONF_NO_PERSON_DELAY): number.number_schema(
            C4004NoPersonDelayNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:timer-off-outline",
            unit_of_measurement="s",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4004_ID])

    if conf := config.get(CONF_INSTALL_HEIGHT):
        n = await number.new_number(conf, min_value=0, max_value=500, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_install_height_number(n))

    if conf := config.get(CONF_INSTALL_Z_ANGLE):
        n = await number.new_number(conf, min_value=0, max_value=90, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_install_z_angle_number(n))

    if conf := config.get(CONF_RANGE_X_MAX):
        n = await number.new_number(conf, min_value=-32767, max_value=32767, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_range_x_max_number(n))

    if conf := config.get(CONF_RANGE_X_MIN):
        n = await number.new_number(conf, min_value=-32767, max_value=32767, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_range_x_min_number(n))

    if conf := config.get(CONF_RANGE_Y_MAX):
        n = await number.new_number(conf, min_value=-32767, max_value=32767, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_range_y_max_number(n))

    if conf := config.get(CONF_RANGE_Y_MIN):
        n = await number.new_number(conf, min_value=-32767, max_value=32767, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_range_y_min_number(n))

    if conf := config.get(CONF_TARGET_COUNT):
        n = await number.new_number(conf, min_value=0, max_value=8, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_target_count_number(n))

    if conf := config.get(CONF_PEOPLE_REPORT_INTERVAL):
        n = await number.new_number(conf, min_value=0, max_value=3600, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_people_report_interval_number(n))

    if conf := config.get(CONF_TRAJECTORY_GENERATE_DISTANCE):
        n = await number.new_number(conf, min_value=0, max_value=1000, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_trajectory_generate_distance_number(n))

    if conf := config.get(CONF_TRAJECTORY_HOLD_TIME):
        n = await number.new_number(conf, min_value=0, max_value=3600, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_trajectory_hold_time_number(n))

    if conf := config.get(CONF_NO_PERSON_DELAY):
        n = await number.new_number(conf, min_value=0, max_value=3600, step=1)
        await cg.register_parented(n, config[CONF_C4004_ID])
        cg.add(parent.set_no_person_delay_number(n))
