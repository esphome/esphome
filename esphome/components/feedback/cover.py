from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_CLOSE_ENDSTOP,
    CONF_MAX_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_OPEN_ENDSTOP,
    CONF_STOP_ACTION,
    CONF_UPDATE_INTERVAL,
)

CONF_OPEN_SENSOR = "open_sensor"
CONF_CLOSE_SENSOR = "close_sensor"
CONF_OPEN_OBSTACLE_SENSOR = "open_obstacle_sensor"
CONF_CLOSE_OBSTACLE_SENSOR = "close_obstacle_sensor"
CONF_HAS_BUILT_IN_ENDSTOP = "has_built_in_endstop"
CONF_INFER_ENDSTOP_FROM_MOVEMENT = "infer_endstop_from_movement"
CONF_DIRECTION_CHANGE_WAIT_TIME = "direction_change_wait_time"
CONF_ACCELERATION_WAIT_TIME = "acceleration_wait_time"
CONF_OBSTACLE_ROLLBACK = "obstacle_rollback"

endstop_ns = cg.esphome_ns.namespace("feedback")
FeedbackCover = endstop_ns.class_("FeedbackCover", cover.Cover, cg.Component)


def validate_infer_endstop(config):
    if config[CONF_INFER_ENDSTOP_FROM_MOVEMENT] is True:
        if config[CONF_HAS_BUILT_IN_ENDSTOP] is False:
            raise cv.Invalid(
                f"{CONF_INFER_ENDSTOP_FROM_MOVEMENT} can only be set if {CONF_HAS_BUILT_IN_ENDSTOP} is also set"
            )

        if CONF_OPEN_SENSOR not in config:
            raise cv.Invalid(
                f"{CONF_INFER_ENDSTOP_FROM_MOVEMENT} cannot be set if movement sensors are not supplied"
            )

        if CONF_OPEN_ENDSTOP in config or CONF_CLOSE_ENDSTOP in config:
            raise cv.Invalid(
                f"{CONF_INFER_ENDSTOP_FROM_MOVEMENT} cannot be set if endstop sensors are supplied"
            )

    return config


CONFIG_FEEDBACK_COVER_BASE_SCHEMA = (
    cover.cover_schema(FeedbackCover)
    .extend(
        {
            cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OPEN_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_OPEN_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_OPEN_OBSTACLE_SENSOR): cv.use_id(
                binary_sensor.BinarySensor
            ),
            cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CLOSE_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_CLOSE_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_CLOSE_OBSTACLE_SENSOR): cv.use_id(
                binary_sensor.BinarySensor
            ),
            cv.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_HAS_BUILT_IN_ENDSTOP, default=False): cv.boolean,
            cv.Optional(CONF_ASSUMED_STATE): cv.boolean,
            cv.Optional(
                CONF_UPDATE_INTERVAL, "1000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_INFER_ENDSTOP_FROM_MOVEMENT, False): cv.boolean,
            cv.Optional(
                CONF_DIRECTION_CHANGE_WAIT_TIME
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_ACCELERATION_WAIT_TIME, "0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OBSTACLE_ROLLBACK, default="10%"): cv.percentage,
        },
    )
    .extend(cv.COMPONENT_SCHEMA)
)


CONFIG_SCHEMA = cv.All(
    CONFIG_FEEDBACK_COVER_BASE_SCHEMA,
    cv.has_none_or_all_keys(CONF_OPEN_SENSOR, CONF_CLOSE_SENSOR),
    validate_infer_endstop,
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    # STOP
    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    # OPEN
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    if CONF_OPEN_ENDSTOP in config:
        bin = await cg.get_variable(config[CONF_OPEN_ENDSTOP])
        cg.add(var.set_open_endstop(bin))
    if CONF_OPEN_SENSOR in config:
        bin = await cg.get_variable(config[CONF_OPEN_SENSOR])
        cg.add(var.set_open_sensor(bin))
    if CONF_OPEN_OBSTACLE_SENSOR in config:
        bin = await cg.get_variable(config[CONF_OPEN_OBSTACLE_SENSOR])
        cg.add(var.set_open_obstacle_sensor(bin))

    # CLOSE
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    if CONF_CLOSE_ENDSTOP in config:
        bin = await cg.get_variable(config[CONF_CLOSE_ENDSTOP])
        cg.add(var.set_close_endstop(bin))
    if CONF_CLOSE_SENSOR in config:
        bin = await cg.get_variable(config[CONF_CLOSE_SENSOR])
        cg.add(var.set_close_sensor(bin))
    if CONF_CLOSE_OBSTACLE_SENSOR in config:
        bin = await cg.get_variable(config[CONF_CLOSE_OBSTACLE_SENSOR])
        cg.add(var.set_close_obstacle_sensor(bin))

    # OTHER
    if CONF_MAX_DURATION in config:
        cg.add(var.set_max_duration(config[CONF_MAX_DURATION]))

    cg.add(var.set_has_built_in_endstop(config[CONF_HAS_BUILT_IN_ENDSTOP]))

    if CONF_ASSUMED_STATE in config:
        cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
    else:
        cg.add(
            var.set_assumed_state(
                not (
                    (CONF_CLOSE_ENDSTOP in config and CONF_OPEN_ENDSTOP in config)
                    or config[CONF_INFER_ENDSTOP_FROM_MOVEMENT]
                )
            )
        )

    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_infer_endstop(config[CONF_INFER_ENDSTOP_FROM_MOVEMENT]))
    if CONF_DIRECTION_CHANGE_WAIT_TIME in config:
        cg.add(
            var.set_direction_change_waittime(config[CONF_DIRECTION_CHANGE_WAIT_TIME])
        )
    cg.add(var.set_acceleration_wait_time(config[CONF_ACCELERATION_WAIT_TIME]))
    cg.add(var.set_obstacle_rollback(config[CONF_OBSTACLE_ROLLBACK]))
