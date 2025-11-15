from esphome import automation
import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_STOP_ACTION,
)

CODEOWNERS = ["@klaudiusz223"]

time_based_tilt_ns = cg.esphome_ns.namespace("time_based_tilt")
TimeBasedTiltCover = time_based_tilt_ns.class_(
    "TimeBasedTiltCover", cover.Cover, cg.Component
)

CONF_TILT_OPEN_DURATION = "tilt_open_duration"
CONF_TILT_CLOSE_DURATION = "tilt_close_duration"
CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"
CONF_RECALIBRATION_OPEN_TIME = "recalibration_open_time"
CONF_RECALIBRATION_CLOSE_TIME = "recalibration_close_time"
CONF_ACTUATOR_ACTIVATION_OPEN_TIME = "actuator_activation_open_time"
CONF_ACTUATOR_ACTIVATION_CLOSE_TIME = "actuator_activation_close_time"
CONF_INERTIA_OPEN_TIME = "inertia_open_time"
CONF_INERTIA_CLOSE_TIME = "inertia_close_time"

CONFIG_SCHEMA = (
    cover.cover_schema(TimeBasedTiltCover)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(TimeBasedTiltCover),
            cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ASSUMED_STATE, default=True): cv.boolean,
            cv.Optional(
                CONF_TILT_OPEN_DURATION, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_TILT_CLOSE_DURATION, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_INTERLOCK_WAIT_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_RECALIBRATION_OPEN_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_RECALIBRATION_CLOSE_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_INERTIA_OPEN_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_INERTIA_CLOSE_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_ACTUATOR_ACTIVATION_OPEN_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_ACTUATOR_ACTIVATION_CLOSE_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    cg.add(var.set_tilt_open_duration(config[CONF_TILT_OPEN_DURATION]))
    cg.add(var.set_tilt_close_duration(config[CONF_TILT_CLOSE_DURATION]))
    cg.add(var.set_interlock_wait_time(config[CONF_INTERLOCK_WAIT_TIME]))
    cg.add(var.set_recalibration_open_time(config[CONF_RECALIBRATION_OPEN_TIME]))
    cg.add(var.set_recalibration_close_time(config[CONF_RECALIBRATION_CLOSE_TIME]))
    cg.add(var.set_inertia_close_time(config[CONF_INERTIA_CLOSE_TIME]))
    cg.add(var.set_inertia_open_time(config[CONF_INERTIA_OPEN_TIME]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
    cg.add(
        var.set_actuator_activation_open_time(
            config[CONF_ACTUATOR_ACTIVATION_OPEN_TIME]
        )
    )
    cg.add(
        var.set_actuator_activation_close_time(
            config[CONF_ACTUATOR_ACTIVATION_CLOSE_TIME]
        )
    )
